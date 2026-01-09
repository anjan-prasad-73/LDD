/* vblock.c */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/semaphore.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/file.h>
#include <linux/fcntl.h>
#include <linux/init.h>

#include "vblock_ioctl.h"

#define DEVICE_NAME     "vblock"
#define CLASS_NAME      "vblock"

/* --- Storage ------------------------------------------------------- */

static u8 vblock_data[VBLOCK_SIZE];
static u8 vblock_mirror[VBLOCK_SIZE];   /* used only if mirror_enable != 0 */

/* Region lock state: bit i set => region i locked */
static u8 region_lock_bitmap;           /* 8 bits for 8 regions */

/* Per-region mutex: protects writes/lock/unlock/erase/mirror for that region */
static struct mutex region_mutex[VBLOCK_NUM_REGIONS];

/* Semaphore for region read operations + backup coordination */
static struct semaphore vblock_read_sem;

/* --- Module parameters --------------------------------------------- */

#define MAX_KEYS 8

static int user_keys[MAX_KEYS];
static int key_count;
module_param_array(user_keys, int, &key_count, 0644);
MODULE_PARM_DESC(user_keys, "Authorized integer keys for write access to locked regions");

static int mirror_enable;
module_param(mirror_enable, int, 0644);
MODULE_PARM_DESC(mirror_enable, "Enable mirroring to secondary 4KB buffer (0=off,1=on)");
int vblock_backup_to_file(const char *path); 
/* --- Char dev bookkeeping ----------------------------------------- */

static dev_t vblock_dev;
static struct cdev vblock_cdev;
static struct class *vblock_class;

/* --- Helpers ------------------------------------------------------- */

static inline bool region_is_locked(int region)
{
    return region_lock_bitmap & (1U << region);
}

static inline void lock_region_bit(int region)
{
    region_lock_bitmap |= (1U << region);
}

static inline void unlock_region_bit(int region)
{
    region_lock_bitmap &= ~(1U << region);
}

static bool key_is_authorized(int key)
{
    int i;
    for (i = 0; i < key_count; ++i) {
        if (user_keys[i] == key)
            return true;
    }
    return false;
}

/* --- File operations ----------------------------------------------- */

static int vblock_open(struct inode *inode, struct file *filp)
{
    /* nothing special; could track opens here if you want */
    return 0;
}

static int vblock_release(struct inode *inode, struct file *filp)
{
    return 0;
}

/* llseek to support arbitrary offsets */
static loff_t vblock_llseek(struct file *file, loff_t off, int whence)
{
    loff_t newpos = 0;

    switch (whence) {
    case SEEK_SET:
        newpos = off;
        break;
    case SEEK_CUR:
        newpos = file->f_pos + off;
        break;
    case SEEK_END:
        newpos = VBLOCK_SIZE + off;
        break;
    default:
        return -EINVAL;
    }

    if (newpos < 0 || newpos > VBLOCK_SIZE)
        return -EINVAL;

    file->f_pos = newpos;
    return newpos;
}

/* Arbitrary read: always allowed, ignores lock state */
static ssize_t vblock_read(struct file *filp, char __user *buf,
                           size_t count, loff_t *ppos)
{
    loff_t pos = *ppos;
    size_t to_copy;
    size_t remaining = count;
    int region;

    if (pos >= VBLOCK_SIZE)
        return 0;

    if (pos + count > VBLOCK_SIZE)
        to_copy = VBLOCK_SIZE - pos;
    else
        to_copy = count;

    if (to_copy == 0)
        return 0;

    /* We want some region-level synchronization.
     * For each region touched, take that region's mutex while copying.
     * Reads are protected against concurrent writers at region granularity.
     */

    size_t done = 0;

    while (remaining && pos < VBLOCK_SIZE) {
        region = pos / VBLOCK_REGION_SIZE;
        size_t region_offset = pos % VBLOCK_REGION_SIZE;
        size_t region_left = VBLOCK_REGION_SIZE - region_offset;
        size_t chunk = min(remaining, region_left);

        if (pos + chunk > VBLOCK_SIZE)
            chunk = VBLOCK_SIZE - pos;

        mutex_lock(&region_mutex[region]);

        if (copy_to_user(buf + done, &vblock_data[pos], chunk)) {
            mutex_unlock(&region_mutex[region]);
            return -EFAULT;
        }

        mutex_unlock(&region_mutex[region]);

        pos += chunk;
        done += chunk;
        remaining -= chunk;
    }

    *ppos = pos;

    return done;
}

/*
 * Write format when region may be locked:
 *   "<key>:<offset>:<data>"
 *
 * If region locked:
 *   - key must be present and must match one of module_param_array user_keys[]
 * If region unlocked:
 *   - key may be omitted by using:
 *       "<offset>:<data>"
 *
 * offset is a global byte offset (0..4095).
 * data length must NOT cross region boundary.
 */
static ssize_t vblock_write(struct file *filp, const char __user *buf,
                            size_t count, loff_t *ppos)
{
    char *kbuf, *p;
    char *first, *second;
    char *data_str;
    int key = -1;
    bool key_present = false;
    unsigned int offset;
    size_t data_len;
    int region;
    int ret;

    if (count == 0)
        return 0;

    if (count > 1023) /* arbitrary sanity limit */
        return -EINVAL;

    kbuf = kzalloc(count + 1, GFP_KERNEL);
    if (!kbuf)
        return -ENOMEM;

    if (copy_from_user(kbuf, buf, count)) {
        kfree(kbuf);
        return -EFAULT;
    }
    kbuf[count] = '\0';

    p = kbuf;

    /* Split on ':' */
    first = strsep(&p, ":");
    if (!first) {
        ret = -EINVAL;
        goto out;
    }

    second = strsep(&p, ":");
    if (!second) {
        /* Need at least two fields */
        ret = -EINVAL;
        goto out;
    }

    if (p) {
        /* Form: key:offset:data */
        data_str = p;
        key_present = true;
        if (kstrtoint(first, 10, &key)) {
            ret = -EINVAL;
            goto out;
        }
        if (kstrtouint(second, 10, &offset)) {
            ret = -EINVAL;
            goto out;
        }
    } else {
        /* Form: offset:data (no key) */
        data_str = second;
        key_present = false;
        if (kstrtouint(first, 10, &offset)) {
            ret = -EINVAL;
            goto out;
        }
    }

    data_len = strlen(data_str);

    if (offset >= VBLOCK_SIZE) {
        ret = -EINVAL;
        goto out;
    }
    if (data_len == 0) {
        ret = 0;
        goto out;
    }
    if (offset + data_len > VBLOCK_SIZE) {
        /* prevent overrun */
        ret = -EINVAL;
        goto out;
    }

    /* Region boundary check: reject writes that cross regions */
    region = offset / VBLOCK_REGION_SIZE;
    if (((offset + data_len - 1) / VBLOCK_REGION_SIZE) != region) {
        /* Either reject or implement region-splitting; we choose reject. */
        ret = -EINVAL;
        goto out;
    }

    /* Check lock + key logic */
    if (region_is_locked(region)) {
        if (!key_present || !key_is_authorized(key)) {
            ret = -EACCES; /* or -EPERM */
            goto out;
        }
    }

    /* Now do the actual write with region-level locking */
    mutex_lock(&region_mutex[region]);

    memcpy(&vblock_data[offset], data_str, data_len);

    if (mirror_enable)
        memcpy(&vblock_mirror[offset], data_str, data_len);

    mutex_unlock(&region_mutex[region]);

    /* We report full count consumed (what user wrote) */
    *ppos = offset + data_len;
    ret = count;

out:
    kfree(kbuf);
    return ret;
}

/* --- IOCTL Handler ------------------------------------------------- */

static long vblock_ioctl(struct file *filp,
                         unsigned int cmd, unsigned long arg)
{
    int region;
    int __user *argp_int = (int __user *)arg;

    switch (cmd) {

    case VBLOCK_LOCK_REGION:
        if (get_user(region, argp_int))
            return -EFAULT;
        if (region < 0 || region >= VBLOCK_NUM_REGIONS)
            return -EINVAL;

        mutex_lock(&region_mutex[region]);
        lock_region_bit(region);
        mutex_unlock(&region_mutex[region]);
        return 0;

    case VBLOCK_UNLOCK_REGION:
        if (get_user(region, argp_int))
            return -EFAULT;
        if (region < 0 || region >= VBLOCK_NUM_REGIONS)
            return -EINVAL;

        mutex_lock(&region_mutex[region]);
        unlock_region_bit(region);
        mutex_unlock(&region_mutex[region]);
        return 0;

    case VBLOCK_READ_REGION: {
        struct vblock_region kregion;

        if (copy_from_user(&kregion, (void __user *)arg, sizeof(kregion)))
            return -EFAULT;

        if (kregion.region_index >= VBLOCK_NUM_REGIONS)
            return -EINVAL;

        /* Coordinate bulk region reads via semaphore */
        if (down_interruptible(&vblock_read_sem))
            return -ERESTARTSYS;

        mutex_lock(&region_mutex[kregion.region_index]);
        memcpy(kregion.data,
               &vblock_data[kregion.region_index * VBLOCK_REGION_SIZE],
               VBLOCK_REGION_SIZE);
        mutex_unlock(&region_mutex[kregion.region_index]);

        up(&vblock_read_sem);

        if (copy_to_user((void __user *)arg, &kregion, sizeof(kregion)))
            return -EFAULT;

        return 0;
    }

    case VBLOCK_GET_INFO: {
        struct vblock_info info;

        info.size        = VBLOCK_SIZE;
        info.region_size = VBLOCK_REGION_SIZE;
        info.num_regions = VBLOCK_NUM_REGIONS;
        info.lock_bitmap = region_lock_bitmap;

        if (copy_to_user((void __user *)arg, &info, sizeof(info)))
            return -EFAULT;

        return 0;
    }

case VBLOCK_READ_MIRROR: {
    struct vblock_region r;

    if (copy_from_user(&r, (void __user *)arg, sizeof(r)))
        return -EFAULT;

    if (r.region_index < 0 || r.region_index >= VBLOCK_NUM_REGIONS)
        return -EINVAL;

    mutex_lock(&region_mutex[r.region_index]);

    memcpy(r.data,
           &vblock_mirror[r.region_index * VBLOCK_REGION_SIZE],
           VBLOCK_REGION_SIZE);

    mutex_unlock(&region_mutex[r.region_index]);

    if (copy_to_user((void __user *)arg, &r, sizeof(r)))
        return -EFAULT;

    return 0;
}

case VBLOCK_BACKUP: {
    char path[256];

    if (copy_from_user(path, (char __user *)arg, sizeof(path)))
        return -EFAULT;

    path[255] = '\0';

    return vblock_backup_to_file(path);
}

    case VBLOCK_ERASE_REGION:
        if (get_user(region, argp_int))
            return -EFAULT;
        if (region < 0 || region >= VBLOCK_NUM_REGIONS)
            return -EINVAL;

        mutex_lock(&region_mutex[region]);

        memset(&vblock_data[region * VBLOCK_REGION_SIZE], 0,
               VBLOCK_REGION_SIZE);

        if (mirror_enable)
            memset(&vblock_mirror[region * VBLOCK_REGION_SIZE], 0,
                   VBLOCK_REGION_SIZE);

        mutex_unlock(&region_mutex[region]);

        return 0;

    default:
        return -ENOTTY;
    }
}

/* --- Exported backup API -------------------------------------------
 *
 * int vblock_backup_to_file(const char *path)
 *   - path: kernel-space string with absolute path
 *   - returns 0 on success or negative errno
 *
 * This is EXPORT_SYMBOL so other kernel modules can trigger backup.
 */

int vblock_backup_to_file(const char *path)
{
    struct file *filp;
    loff_t pos = 0;
    ssize_t written;
    int ret = 0;
    u8 *tmp;
    int i;

    if (!path)
        return -EINVAL;

    tmp = kmalloc(VBLOCK_SIZE, GFP_KERNEL);
    if (!tmp)
        return -ENOMEM;

    /* Coordinate with VBLOCK_READ_REGION via semaphore
     * and with writers via per-region mutexes.
     */
    if (down_interruptible(&vblock_read_sem)) {
        kfree(tmp);
        return -ERESTARTSYS;
    }

    for (i = 0; i < VBLOCK_NUM_REGIONS; ++i) {
        mutex_lock(&region_mutex[i]);
        memcpy(tmp + i * VBLOCK_REGION_SIZE,
               vblock_data + i * VBLOCK_REGION_SIZE,
               VBLOCK_REGION_SIZE);
        mutex_unlock(&region_mutex[i]);
    }

    up(&vblock_read_sem);

    filp = filp_open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (IS_ERR(filp)) {
        ret = PTR_ERR(filp);
        kfree(tmp);
        return ret;
    }

    written = kernel_write(filp, tmp, VBLOCK_SIZE, &pos);
    if (written < 0 || written != VBLOCK_SIZE) {
        if (written < 0)
            ret = (int)written;
        else
            ret = -EIO;
    }

    filp_close(filp, NULL);
    kfree(tmp);
    return ret;
}
EXPORT_SYMBOL(vblock_backup_to_file);

/* --- File operations table ----------------------------------------- */

static const struct file_operations vblock_fops = {
    .owner          = THIS_MODULE,
    .open           = vblock_open,
    .release        = vblock_release,
    .read           = vblock_read,
    .write          = vblock_write,
    .unlocked_ioctl = vblock_ioctl,
    .llseek         = vblock_llseek,
};

/* --- Init / Exit --------------------------------------------------- */

static int __init vblock_init(void)
{
    int ret, i;

    ret = alloc_chrdev_region(&vblock_dev, 0, 1, DEVICE_NAME);
    if (ret)
        return ret;

    cdev_init(&vblock_cdev, &vblock_fops);
    vblock_cdev.owner = THIS_MODULE;

    ret = cdev_add(&vblock_cdev, vblock_dev, 1);
    if (ret)
        goto err_unregister;

    vblock_class = class_create( CLASS_NAME);
    if (IS_ERR(vblock_class)) {
        ret = PTR_ERR(vblock_class);
        goto err_cdev;
    }

    if (!device_create(vblock_class, NULL, vblock_dev, NULL,
                       DEVICE_NAME "0")) {
        ret = -ENOMEM;
        goto err_class;
    }

    memset(vblock_data, 0, sizeof(vblock_data));
    memset(vblock_mirror, 0, sizeof(vblock_mirror));
    region_lock_bitmap = 0;

    for (i = 0; i < VBLOCK_NUM_REGIONS; ++i)
        mutex_init(&region_mutex[i]);

    sema_init(&vblock_read_sem, 1);

    pr_info("vblock: loaded (major=%d minor=%d), keys=%d, mirror=%d\n",
            MAJOR(vblock_dev), MINOR(vblock_dev),
            key_count, mirror_enable);

    return 0;

err_class:
    class_destroy(vblock_class);
err_cdev:
    cdev_del(&vblock_cdev);
err_unregister:
    unregister_chrdev_region(vblock_dev, 1);
    return ret;
}

static void __exit vblock_exit(void)
{
    device_destroy(vblock_class, vblock_dev);
    class_destroy(vblock_class);
    cdev_del(&vblock_cdev);
    unregister_chrdev_region(vblock_dev, 1);

    pr_info("vblock: unloaded\n");
}

module_init(vblock_init);
module_exit(vblock_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("You");
MODULE_DESCRIPTION("Simulated 4KB virtual block device with region locks, keys, IOCTLs, and backup");
MODULE_VERSION("1.0");

