
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/ioctl.h>

/* Motor modes */
enum motor_mode {
    MOTOR_MODE_NORMAL = 0,
    MOTOR_MODE_REVERSE,
    MOTOR_MODE_BRAKE,
};

/* Returned by read() */
struct motor_state {
    int speed;      /* arbitrary speed units */
    int mode;       /* one of enum motor_mode */
};

/* IOCTL definitions */
#define MOTOR_IOCTL_MAGIC   'M'
#define MOTOR_IOCTL_SET_MODE  _IOW(MOTOR_IOCTL_MAGIC, 1, int)
#define MOTOR_IOCTL_GET_MODE  _IOR(MOTOR_IOCTL_MAGIC, 2, int)



#define DRIVER_NAME "motor_driver"
#define DEVICE_NAME "motor0"

static dev_t motor_dev;
static struct cdev motor_cdev;
static struct class *motor_class;

struct motor_device {
    int speed;
    int mode;
    struct mutex lock;
};

static struct motor_device motor;

/* module_param: default motor mode */
static int default_mode = MOTOR_MODE_NORMAL;
module_param(default_mode, int, 0644);
MODULE_PARM_DESC(default_mode, "Default motor mode (0=NORMAL, 1=REVERSE, 2=BRAKE)");

/* ---- exported symbol: can be used by another kernel module ---- */
void motor_set_speed(int new_speed)
{
    mutex_lock(&motor.lock);
    motor.speed = new_speed;
    pr_info("motor_driver: speed set via exported symbol: %d\n", new_speed);
    mutex_unlock(&motor.lock);
}
EXPORT_SYMBOL(motor_set_speed);
/* -------------------------------------------------------------- */

/* file operations */

static int motor_open(struct inode *inode, struct file *filp)
{
    /* You can store private data if needed */
    return 0;
}

static int motor_release(struct inode *inode, struct file *filp)
{
    return 0;
}

/*
 * write() sets speed (stored in kernel).
 * Expecting binary int from user.
 */
static ssize_t motor_write(struct file *filp, const char __user *buf,
                           size_t len, loff_t *off)
{
    int new_speed;

    if (len < sizeof(int))
        return -EINVAL;

    if (copy_from_user(&new_speed, buf, sizeof(int)))
        return -EFAULT;

    mutex_lock(&motor.lock);
    motor.speed = new_speed;
    mutex_unlock(&motor.lock);

    pr_info("motor_driver: speed set via write(): %d\n", new_speed);
    return sizeof(int);
}

/*
 * read() returns (speed + mode) packed in struct motor_state
 */
static ssize_t motor_read(struct file *filp, char __user *buf,
                          size_t len, loff_t *off)
{
    struct motor_state state;

    if (len < sizeof(struct motor_state))
        return -EINVAL;

    /* Simple read-once behaviour */
    if (*off >= sizeof(struct motor_state))
        return 0;

    mutex_lock(&motor.lock);
    state.speed = motor.speed;
    state.mode  = motor.mode;
    mutex_unlock(&motor.lock);

    if (copy_to_user(buf, &state, sizeof(state)))
        return -EFAULT;

    *off += sizeof(state);
    return sizeof(state);
}

/*
 * IOCTL sets/gets mode: NORMAL / REVERSE / BRAKE.
 */
static long motor_unlocked_ioctl(struct file *filp,
                                 unsigned int cmd, unsigned long arg)
{
    int mode;

    switch (cmd) {
    case MOTOR_IOCTL_SET_MODE:
        if (copy_from_user(&mode, (int __user *)arg, sizeof(int)))
            return -EFAULT;

        if (mode < MOTOR_MODE_NORMAL || mode > MOTOR_MODE_BRAKE)
            return -EINVAL;

        mutex_lock(&motor.lock);
        motor.mode = mode;
        mutex_unlock(&motor.lock);

        pr_info("motor_driver: mode set via ioctl(): %d\n", mode);
        break;

    case MOTOR_IOCTL_GET_MODE:
        mutex_lock(&motor.lock);
        mode = motor.mode;
        mutex_unlock(&motor.lock);

        if (copy_to_user((int __user *)arg, &mode, sizeof(int)))
            return -EFAULT;
        break;

    default:
        return -ENOTTY;
    }

    return 0;
}

static const struct file_operations motor_fops = {
    .owner          = THIS_MODULE,
    .open           = motor_open,
    .release        = motor_release,
    .read           = motor_read,
    .write          = motor_write,
    .unlocked_ioctl = motor_unlocked_ioctl,
};

/* Module init/exit */

static int __init motor_init(void)
{
    int ret;

    /* Allocate device number */
    ret = alloc_chrdev_region(&motor_dev, 0, 1, DRIVER_NAME);
    if (ret < 0) {
        pr_err("motor_driver: alloc_chrdev_region failed\n");
        return ret;
    }

    /* Init cdev */
    cdev_init(&motor_cdev, &motor_fops);
    motor_cdev.owner = THIS_MODULE;

    ret = cdev_add(&motor_cdev, motor_dev, 1);
    if (ret < 0) {
        pr_err("motor_driver: cdev_add failed\n");
        unregister_chrdev_region(motor_dev, 1);
        return ret;
    }

    /* Create class & device node /dev/motor0 */
    motor_class = class_create( DRIVER_NAME);
    if (IS_ERR(motor_class)) {
        pr_err("motor_driver: class_create failed\n");
        cdev_del(&motor_cdev);
        unregister_chrdev_region(motor_dev, 1);
        return PTR_ERR(motor_class);
    }

    if (IS_ERR(device_create(motor_class, NULL, motor_dev, NULL, DEVICE_NAME))) {
        pr_err("motor_driver: device_create failed\n");
        class_destroy(motor_class);
        cdev_del(&motor_cdev);
        unregister_chrdev_region(motor_dev, 1);
        return -ENOMEM;
    }

    /* Initialize motor state */
    mutex_init(&motor.lock);
    motor.speed = 0;
    motor.mode  = default_mode;

    pr_info("motor_driver: loaded. Major=%d, default_mode=%d (/dev/%s)\n",
            MAJOR(motor_dev), default_mode, DEVICE_NAME);

    return 0;
}

static void __exit motor_exit(void)
{
    device_destroy(motor_class, motor_dev);
    class_destroy(motor_class);
    cdev_del(&motor_cdev);
    unregister_chrdev_region(motor_dev, 1);

    pr_info("motor_driver: unloaded\n");
}

module_init(motor_init);
module_exit(motor_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anjan prasad");
MODULE_DESCRIPTION("Simple motor control char driver with IOCTL + exported symbol");

