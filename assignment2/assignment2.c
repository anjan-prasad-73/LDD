#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/atomic.h>
#include <linux/wait.h>
#include <linux/random.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/slab.h>
#include <linux/ktime.h>

#define DRV_NAME        "temp_sensor_alerts"

/* ---------- IOCTLs ---------- */

#define TEMP_IOC_MAGIC          'T'

/* Set thresholds: argument is struct temp_thresholds (user space) */
#define TEMP_IOC_SET_THRESHOLDS _IOW(TEMP_IOC_MAGIC, 1, struct temp_thresholds)

/* Non-blocking: get current state (no wait) */
#define TEMP_IOC_GET_STATE      _IOR(TEMP_IOC_MAGIC, 2, struct temp_state)

/* ---------- Types visible to user also ---------- */

/* Alert type */
enum temp_alert_type {
    ALERT_NONE = 0,
    ALERT_LOW  = 1,
    ALERT_HIGH = 2,
};

/* thresholds in milli-degree C */
struct temp_thresholds {
    int low_mdeg;
    int high_mdeg;
};

/* state for GET_STATE ioctl */
struct temp_state {
    int  current_temp_mdeg;
    int  low_mdeg;
    int  high_mdeg;
    enum temp_alert_type last_alert_type;
};

/* what read() returns for each alert */
struct temp_alert_user {
    int  alert_id;        /* monotonically increasing */
    int  temp_mdeg;
    enum temp_alert_type type;
};

/* ---------- Internal sensor structure (as in assignment) ---------- */

#define ALERT_HISTORY_SIZE 10
#define DEBOUNCE_MS        500     /* ignore alert flips quicker than this */

struct alert_event {
    int id;
    int temp_mdeg;
    enum temp_alert_type type;
    ktime_t ts;
};

struct temp_sensor {
    atomic_t current_temp;     /* in milli-degrees C          */
    atomic_t high_threshold;
    atomic_t low_threshold;
    atomic_t alert_count;      /* number of alerts generated */

    wait_queue_head_t alert_queue;

    /* history + debounce need lock (multiple fields together) */
    spinlock_t lock;
    struct alert_event history[ALERT_HISTORY_SIZE];
    int last_alert_id;
    enum temp_alert_type last_type;
    unsigned long last_change_jiffies;
};

/* Per-open file context: for “no missed events” behaviour */
struct temp_file_ctx {
    int last_seen_alert_id;
};

static struct temp_sensor g_sensor;

/* Timer to simulate sensor updates */
static struct timer_list temp_timer;

/* ---------- Helper: push alert into history & wake waiters ---------- */

static void temp_sensor_push_alert(struct temp_sensor *s,
                                   int temp_mdeg,
                                   enum temp_alert_type type)
{
    unsigned long flags;
    struct alert_event *ev;
    int id;

    /* increment global alert count atomically */
    id = atomic_inc_return(&s->alert_count);

    spin_lock_irqsave(&s->lock, flags);

    s->last_alert_id = id;
    s->last_type = type;

    ev = &s->history[id % ALERT_HISTORY_SIZE];
    ev->id        = id;
    ev->temp_mdeg = temp_mdeg;
    ev->type      = type;
    ev->ts        = ktime_get();

    spin_unlock_irqrestore(&s->lock, flags);

    /* wake up all waiters – they’ll check their own last_seen_alert_id */
    wake_up_interruptible(&s->alert_queue);
}

/* ---------- Timer callback: simulate temperature + generate alerts ---------- */

static void temp_timer_fn(struct timer_list *t)
{
    int temp_mdeg;
    int high, low;
    enum temp_alert_type new_type = ALERT_NONE;
    struct temp_sensor *s = &g_sensor;
    unsigned long now = jiffies;

    /* Simulate 0–80°C in milli-degrees */
    get_random_bytes(&temp_mdeg, sizeof(temp_mdeg));
    if (temp_mdeg < 0)
        temp_mdeg = -temp_mdeg;
    temp_mdeg %= (80 * 1000);        /* 0 .. 80000 mdeg */

    atomic_set(&s->current_temp, temp_mdeg);

    high = atomic_read(&s->high_threshold);
    low  = atomic_read(&s->low_threshold);

    if (temp_mdeg >= high)
        new_type = ALERT_HIGH;
    else if (temp_mdeg <= low)
        new_type = ALERT_LOW;
    else
        new_type = ALERT_NONE;

    /* Debounce: only generate alert when state changes and is spaced out */
    if (new_type != s->last_type) {
        if (time_after(now, s->last_change_jiffies +
                            msecs_to_jiffies(DEBOUNCE_MS))) {
            s->last_change_jiffies = now;
            if (new_type != ALERT_NONE) {
                pr_info(DRV_NAME ": ALERT %s at %d mdegC\n",
                        (new_type == ALERT_HIGH) ? "HIGH" : "LOW",
                        temp_mdeg);
                temp_sensor_push_alert(s, temp_mdeg, new_type);
            }
            s->last_type = new_type;
        }
    }

    /* re-arm timer (1 second) */
    mod_timer(&temp_timer, jiffies + HZ);
}

/* ---------- File operations ---------- */

static int temp_open(struct inode *inode, struct file *file)
{
    struct temp_file_ctx *ctx;

    ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
    if (!ctx)
        return -ENOMEM;

    /* start from “no alerts seen” */
    ctx->last_seen_alert_id = atomic_read(&g_sensor.alert_count);
    file->private_data = ctx;

    return 0;
}

static int temp_release(struct inode *inode, struct file *file)
{
    kfree(file->private_data);
    return 0;
}

/* Blocking read:
 * waits until a *new* alert is generated, then returns one alert_event
 */
static ssize_t temp_read(struct file *file, char __user *buf,
                         size_t len, loff_t *ppos)
{
    struct temp_file_ctx *ctx = file->private_data;
    struct temp_sensor *s = &g_sensor;
    int ret;
    int my_last;
    int cur_alert_id;
    struct alert_event ev_copy;
    unsigned long flags;

    if (len < sizeof(struct temp_alert_user))
        return -EINVAL;

    /* Fast check first (non-sleeping) */
    cur_alert_id = atomic_read(&s->alert_count);
    if (cur_alert_id == ctx->last_seen_alert_id) {

        if (file->f_flags & O_NONBLOCK)
            return -EAGAIN;

        /* wait until alert_count changes */
        ret = wait_event_interruptible(s->alert_queue,
                atomic_read(&s->alert_count) != ctx->last_seen_alert_id);
        if (ret)
            return ret;  /* -ERESTARTSYS if interrupted */
    }

    /* Now there is at least one new alert */
    my_last = ctx->last_seen_alert_id = atomic_read(&s->alert_count);

    /* Fetch from history: newest alert for this id (or nearest) */
    spin_lock_irqsave(&s->lock, flags);
    ev_copy = s->history[my_last % ALERT_HISTORY_SIZE];
    spin_unlock_irqrestore(&s->lock, flags);

    /* prepare user struct */
    {
        struct temp_alert_user out = {
            .alert_id  = ev_copy.id,
            .temp_mdeg = ev_copy.temp_mdeg,
            .type      = ev_copy.type,
        };

        if (copy_to_user(buf, &out, sizeof(out)))
            return -EFAULT;
    }

    return sizeof(struct temp_alert_user);
}

/* ioctl: set thresholds / get non-blocking state */
static long temp_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct temp_sensor *s = &g_sensor;

    switch (cmd) {
    case TEMP_IOC_SET_THRESHOLDS:
        {
            struct temp_thresholds th;

            if (copy_from_user(&th, (void __user *)arg, sizeof(th)))
                return -EFAULT;

            /* Set atomically */
            atomic_set(&s->low_threshold, th.low_mdeg);
            atomic_set(&s->high_threshold, th.high_mdeg);
            pr_info(DRV_NAME ": thresholds set: low=%d, high=%d mdegC\n",
                    th.low_mdeg, th.high_mdeg);
        }
        return 0;

    case TEMP_IOC_GET_STATE:
        {
            struct temp_state st;

            st.current_temp_mdeg = atomic_read(&s->current_temp);
            st.low_mdeg          = atomic_read(&s->low_threshold);
            st.high_mdeg         = atomic_read(&s->high_threshold);
            st.last_alert_type   = s->last_type;

            if (copy_to_user((void __user *)arg, &st, sizeof(st)))
                return -EFAULT;
        }
        return 0;

    default:
        return -ENOTTY;
    }
}

static const struct file_operations temp_fops = {
    .owner          = THIS_MODULE,
    .open           = temp_open,
    .release        = temp_release,
    .read           = temp_read,
    .unlocked_ioctl = temp_ioctl,
    .llseek         = no_llseek,
};

/* Register as misc device: /dev/temp_sensor_alerts */
static struct miscdevice temp_miscdev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = DRV_NAME,
    .fops  = &temp_fops,
};

/* ---------- Module init/exit ---------- */

static int __init temp_sensor_init(void)
{
    int ret;

    /* init sensor struct */
    atomic_set(&g_sensor.current_temp, 25 * 1000); /* 25°C default */
    atomic_set(&g_sensor.low_threshold,  10 * 1000);
    atomic_set(&g_sensor.high_threshold, 40 * 1000);
    atomic_set(&g_sensor.alert_count, 0);

    init_waitqueue_head(&g_sensor.alert_queue);
    spin_lock_init(&g_sensor.lock);
    g_sensor.last_alert_id = 0;
    g_sensor.last_type = ALERT_NONE;
    g_sensor.last_change_jiffies = jiffies;

    ret = misc_register(&temp_miscdev);
    if (ret) {
        pr_err(DRV_NAME ": misc_register failed: %d\n", ret);
        return ret;
    }

    /* start periodic temperature updates */
    timer_setup(&temp_timer, temp_timer_fn, 0);
    mod_timer(&temp_timer, jiffies + HZ);   /* 1 second */

    pr_info(DRV_NAME ": loaded. Device /dev/%s\n", DRV_NAME);
    return 0;
}

static void __exit temp_sensor_exit(void)
{
    del_timer_sync(&temp_timer);
    misc_deregister(&temp_miscdev);
    pr_info(DRV_NAME ": unloaded\n");
}

module_init(temp_sensor_init);
module_exit(temp_sensor_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anjan prasad M");
MODULE_DESCRIPTION("Temperature Sensor with Alerts using atomic_t and wait queues");
// temp_sensor_driver.c
