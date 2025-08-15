#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/slab.h>

#define DEVICE_NAME "mycdrv"
#define CLASS_NAME "my_class"

#define MY_IOCTL_MAGIC 'k'
#define MY_IOCTL_RW(cmd_num, len) _IOWR(MY_IOCTL_MAGIC, (cmd_num), char[len])

static dev_t my_dev;
static struct class *my_class;
static struct cdev my_cdev;

static long my_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    size_t len = _IOC_SIZE(cmd);
    char *kbuf;
    int ret = 0;

    if (_IOC_TYPE(cmd) != MY_IOCTL_MAGIC)
        return -ENOTTY;

    if (len == 0 || len > 4096) // Arbitrary upper limit for safety
        return -EINVAL;

    kbuf = kmalloc(len, GFP_KERNEL);
    if (!kbuf)
        return -ENOMEM;

    // Fill kernel buffer with a message
    snprintf(kbuf, len, "Hello from kernel! (len=%zu)", len);

    // Copy kernel buffer to user buffer
    if (copy_to_user((void __user *)arg, kbuf, len))
        ret = -EFAULT;

    // Optionally, copy user buffer back to kernel buffer (for demonstration)
    if (!ret && copy_from_user(kbuf, (void __user *)arg, len))
        ret = -EFAULT;

    kfree(kbuf);
    return ret;
}

static struct file_operations my_fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = my_ioctl,
};

static int __init my_init(void)
{
    int ret;
    ret = alloc_chrdev_region(&my_dev, 0, 1, DEVICE_NAME);
    if (ret < 0)
        return ret;

    cdev_init(&my_cdev, &my_fops);
    my_cdev.owner = THIS_MODULE;
    ret = cdev_add(&my_cdev, my_dev, 1);
    if (ret < 0)
        goto unregister;

    my_class = class_create(CLASS_NAME);
    if (IS_ERR(my_class))
    {
        ret = PTR_ERR(my_class);
        goto del_cdev;
    }
    device_create(my_class, NULL, my_dev, NULL, DEVICE_NAME);
    pr_info("mycdrv: module loaded\n");
    return 0;

del_cdev:
    cdev_del(&my_cdev);
unregister:
    unregister_chrdev_region(my_dev, 1);
    return ret;
}

static void __exit my_exit(void)
{
    device_destroy(my_class, my_dev);
    class_destroy(my_class);
    cdev_del(&my_cdev);
    unregister_chrdev_region(my_dev, 1);
    pr_info("mycdrv: module unloaded\n");
}

module_init(my_init);
module_exit(my_exit);
MODULE_LICENSE("GPL");