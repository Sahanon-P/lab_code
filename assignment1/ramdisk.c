#include <linux/module.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/ioctl.h>
#include <linux/cdev.h>
#include <linux/device.h>

#define DEVICE_NAME "asgn1"
#define CLASS_NAME "asgn1_class"
#define RAMDISK_IOCTL_SET_MAX_USERS _IOW('r', 1, int)
#define RAMDISK_IOCTL_GET_MAX_USERS _IOR('r', 2, int)

struct ramdisk_page {
    struct list_head list;
    struct page *page;
};

static LIST_HEAD(page_list);
static int max_users = 1;
static int current_users = 0;
static DEFINE_MUTEX(user_mutex);
static DEFINE_MUTEX(page_mutex);
static dev_t dev_no;
static struct cdev ramdisk_cdev;
static struct class *ramdisk_class;
static struct device *ramdisk_device;

static struct page *get_page_at_offset(loff_t offset) {
    struct ramdisk_page *entry;
    loff_t page_offset = offset >> PAGE_SHIFT;
    loff_t current_page = 0;
    
    list_for_each_entry(entry, &page_list, list) {
        if (current_page == page_offset) {
            return entry->page;
        }
        current_page++;
    }
    return NULL;
}

static struct page *allocate_page_at_offset(loff_t offset) {
    struct ramdisk_page *new_entry;
    struct ramdisk_page *entry;
    struct page *new_page;
    loff_t page_offset = offset >> PAGE_SHIFT;
    loff_t current_page = 0;
    
    // Check if page already exists
    list_for_each_entry(entry, &page_list, list) {
        if (current_page == page_offset) {
            return entry->page;
        }
        current_page++;
    }
    
    // Allocate new page entry
    new_entry = kmalloc(sizeof(struct ramdisk_page), GFP_KERNEL);
    if (!new_entry)
        return NULL;
    
    new_page = alloc_page(GFP_KERNEL);
    if (!new_page) {
        kfree(new_entry);
        return NULL;
    }
    
    new_entry->page = new_page;
    
    // Insert pages in order if needed, or just add to tail for simplicity
    list_add_tail(&new_entry->list, &page_list);
    
    return new_page;
}

static int ramdisk_open(struct inode *inode, struct file *filp) {
    mutex_lock(&user_mutex);
    
    if (current_users >= max_users) {
        mutex_unlock(&user_mutex);
        return -EBUSY;
    }
    
    current_users++;
    mutex_unlock(&user_mutex);
    
    printk(KERN_INFO "asgn1: Device opened. Current users: %d\n", current_users);
    return 0;
}

static int ramdisk_release(struct inode *inode, struct file *filp) {
    mutex_lock(&user_mutex);
    current_users--;
    mutex_unlock(&user_mutex);
    
    printk(KERN_INFO "asgn1: Device released. Current users: %d\n", current_users);
    return 0;
}

static ssize_t ramdisk_read(struct file *filp, char __user *buf, size_t count, loff_t *offset) {
    struct page *page;
    void *page_addr;
    size_t bytes_to_read;
    size_t page_offset;
    ssize_t bytes_read = 0;
    
    mutex_lock(&page_mutex);
    
    while (count > 0) {
        page = get_page_at_offset(*offset);
        if (!page) {
            // No page allocated at this offset, return zeros
            break;
        }
        
        page_offset = *offset & (PAGE_SIZE - 1);
        bytes_to_read = min(count, PAGE_SIZE - page_offset);
        
        page_addr = bmap(page);
        if (copy_to_user(buf + bytes_read, page_addr + page_offset, bytes_to_read)) {
            kunmap(page);
            mutex_unlock(&page_mutex);
            return -EFAULT;
        }
        kunmap(page);
        
        *offset += bytes_to_read;
        bytes_read += bytes_to_read;
        count -= bytes_to_read;
    }
    
    mutex_unlock(&page_mutex);
    return bytes_read;
}

static ssize_t ramdisk_write(struct file *filp, const char __user *buf, size_t count, loff_t *offset) {
    struct page *page;
    void *page_addr;
    size_t bytes_to_write;
    size_t page_offset;
    ssize_t bytes_written = 0;
    
    mutex_lock(&page_mutex);
    
    while (count > 0) {
        page = allocate_page_at_offset(*offset);
        if (!page) {
            mutex_unlock(&page_mutex);
            return -ENOMEM;
        }
        
        page_offset = *offset & (PAGE_SIZE - 1);
        bytes_to_write = min(count, PAGE_SIZE - page_offset);
        
        page_addr = bmap(page);
        if (copy_from_user(page_addr + page_offset, buf + bytes_written, bytes_to_write)) {
            kunmap(page);
            mutex_unlock(&page_mutex);
            return -EFAULT;
        }
        kunmap(page);
        
        *offset += bytes_to_write;
        bytes_written += bytes_to_write;
        count -= bytes_to_write;
    }
    
    mutex_unlock(&page_mutex);
    return bytes_written;
}

static loff_t ramdisk_llseek(struct file *filp, loff_t offset, int whence) {
    loff_t new_pos;
    
    switch (whence) {
    case SEEK_SET:
        new_pos = offset;
        break;
    case SEEK_CUR:
        new_pos = filp->f_pos + offset;
        break;
    case SEEK_END:
        // For simplicity, we don't track end of file
        return -EINVAL;
    default:
        return -EINVAL;
    }
    
    if (new_pos < 0)
        return -EINVAL;
    
    filp->f_pos = new_pos;
    return new_pos;
}

static long ramdisk_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {
    int value;
    
    switch (cmd) {
    case RAMDISK_IOCTL_SET_MAX_USERS:
        if (copy_from_user(&value, (int __user *)arg, sizeof(int)))
            return -EFAULT;
        
        if (value <= 0)
            return -EINVAL;
        
        mutex_lock(&user_mutex);
        max_users = value;
        mutex_unlock(&user_mutex);
        
        printk(KERN_INFO "asgn1: Max users set to %d\n", max_users);
        return 0;
        
    case RAMDISK_IOCTL_GET_MAX_USERS:
        mutex_lock(&user_mutex);
        value = max_users;
        mutex_unlock(&user_mutex);
        
        if (copy_to_user((int __user *)arg, &value, sizeof(int)))
            return -EFAULT;
        return 0;
        
    default:
        return -ENOTTY;
    }
}

static struct file_operations ramdisk_fops = {
    .owner = THIS_MODULE,
    .open = ramdisk_open,
    .release = ramdisk_release,
    .read = ramdisk_read,
    .write = ramdisk_write,
    .llseek = ramdisk_llseek,
    .unlocked_ioctl = ramdisk_ioctl,
};

static int __init ramdisk_init(void) {
    int ret;
    
    // Allocate device number
    ret = alloc_chrdev_region(&dev_no, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        printk(KERN_ERR "asgn1: Failed to allocate device number\n");
        return ret;
    }
    
    // Initialize and add character device
    cdev_init(&ramdisk_cdev, &ramdisk_fops);
    ramdisk_cdev.owner = THIS_MODULE;
    
    ret = cdev_add(&ramdisk_cdev, dev_no, 1);
    if (ret < 0) {
        printk(KERN_ERR "asgn1: Failed to add character device\n");
        goto fail_cdev_add;
    }
    
    // Create device class
    ramdisk_class = class_create(CLASS_NAME);
    if (IS_ERR(ramdisk_class)) {
        printk(KERN_ERR "asgn1: Failed to create device class\n");
        ret = PTR_ERR(ramdisk_class);
        goto fail_class_create;
    }
    
    // Create device file
    ramdisk_device = device_create(ramdisk_class, NULL, dev_no, NULL, DEVICE_NAME);
    if (IS_ERR(ramdisk_device)) {
        printk(KERN_ERR "asgn1: Failed to create device\n");
        ret = PTR_ERR(ramdisk_device);
        goto fail_device_create;
    }
    
    printk(KERN_INFO "asgn1: Ramdisk module loaded successfully\n");
    return 0;
    
fail_device_create:
    class_destroy(ramdisk_class);
fail_class_create:
    cdev_del(&ramdisk_cdev);
fail_cdev_add:
    unregister_chrdev_region(dev_no, 1);
    return ret;
}

static void __exit ramdisk_exit(void) {
    struct ramdisk_page *entry, *tmp;
    
    // Remove device and class
    device_destroy(ramdisk_class, dev_no);
    class_destroy(ramdisk_class);
    cdev_del(&ramdisk_cdev);
    unregister_chrdev_region(dev_no, 1);
    
    // Free all allocated pages
    mutex_lock(&page_mutex);
    list_for_each_entry_safe(entry, tmp, &page_list, list) {
        list_del(&entry->list);
        __free_page(entry->page);
        kfree(entry);
    }
    mutex_unlock(&page_mutex);
    
    printk(KERN_INFO "asgn1: Ramdisk module unloaded\n");
}

module_init(ramdisk_init);
module_exit(ramdisk_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Assignment 1 Ramdisk Module");