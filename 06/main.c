#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/cdev.h>

#define DEVICE_NAME "mycdrv"
#define CLASS_NAME "my_class"
#define BUF_SIZE 32

#define MY_IOCTL_MAGIC 'k'
#define MY_IOCTL_RW _IOWR(MY_IOCTL_MAGIC, 1, char[BUF_SIZE])

static char kernel_buf[BUF_SIZE] = "Hello from kernel!";
static dev_t my_dev;
static struct class *my_class;
static struct cdev my_cdev;

static long my_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    if (_IOC_TYPE(cmd) != MY_IOCTL_MAGIC)
        return -ENOTTY;

    switch (cmd)
    {
    case MY_IOCTL_RW:
        if (copy_to_user((void __user *)arg, kernel_buf, BUF_SIZE))
            return -EFAULT;
        if (copy_from_user(kernel_buf, (void __user *)arg, BUF_SIZE))
            return -EFAULT;
        return 0;
    default:
        return -ENOTTY;
    }
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

    my_class = class_create(THIS_MODULE, CLASS_NAME);
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