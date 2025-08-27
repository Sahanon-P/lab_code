
/**
 * File: asgn1.c
 * Date: 13/03/2011
 * Author: Your Name
 * Version: 0.1
 *
 * This is a module which serves as a virtual ramdisk which disk size is
 * limited by the amount of memory available and serves as the requirement for
 * COSC440 assignment 1. This template is provided to students for their
 * convenience and served as hints/tips, but not necessarily as a standard
 * answer for the assignment. So students are free to change any part of
 * the template to fit their design, not the other way around.
 *
 * Note: multiple devices and concurrent modules are not supported in this
 *       version. The template is
 */

/* This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/list.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/device.h>
#include <linux/sched.h>
#include <linux/highmem.h>

#define MYDEV_NAME "asgn1"
#define MYIOC_TYPE 'k'

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("COSC440 asgn1");

/**
 * The node structure for the memory page linked list.
 */
typedef struct page_node_rec
{
  struct list_head list;
  struct page *page;
} page_node;

typedef struct asgn1_dev_t
{
  dev_t dev; /* the device */
  struct cdev *cdev;
  struct list_head mem_list;
  int num_pages;            /* number of memory pages this module currently holds */
  size_t data_size;         /* total data size in this module */
  atomic_t nprocs;          /* number of processes accessing this device */
  atomic_t max_nprocs;      /* max number of processes accessing this device */
  struct kmem_cache *cache; /* cache memory */
  struct class *class;      /* the udev class */
  struct device *device;    /* the udev device node */
} asgn1_dev;

asgn1_dev asgn1_device;

int asgn1_major = 0;     /* major number of module */
int asgn1_minor = 0;     /* minor number of module */
int asgn1_dev_count = 1; /* number of devices */

/**
 * This function frees all memory pages held by the module.
 */
void free_memory_pages(void);
void free_memory_pages(void)
{
  page_node *curr, *tmp;

  /* Loop through the entire page list */
  list_for_each_entry_safe(curr, tmp, &asgn1_device.mem_list, list)
  {
    if (curr->page)
    {
      __free_page(curr->page);
    }
    list_del(&curr->list);
    if (asgn1_device.cache)
    {
      kmem_cache_free(asgn1_device.cache, curr);
    }
    else
    {
      kfree(curr);
    }
  }

  /* reset device data size, and num_pages */
  asgn1_device.data_size = 0;
  asgn1_device.num_pages = 0;
}

/**
 * This function opens the virtual disk, if it is opened in the write-only
 * mode, all memory pages will be freed.
 */
int asgn1_open(struct inode *, struct file *);
int asgn1_open(struct inode *inode, struct file *filp)
{
  /* Increment process count, if exceeds max_nprocs, return -EBUSY */
  if (atomic_read(&asgn1_device.nprocs) >= atomic_read(&asgn1_device.max_nprocs))
  {
    return -EBUSY;
  }

  atomic_inc(&asgn1_device.nprocs);

  /* if opened in write-only mode, free all memory pages */
  if ((filp->f_flags & O_ACCMODE) == O_WRONLY)
  {
    free_memory_pages();
  }

  return 0; /* success */
}

/**
 * This function releases the virtual disk, but nothing needs to be done
 * in this case.
 */
int asgn1_release(struct inode *, struct file *);
int asgn1_release(struct inode *inode, struct file *filp)
{
  /* decrement process count */
  atomic_dec(&asgn1_device.nprocs);
  return 0;
}

/**
 * This function reads contents of the virtual disk and writes to the user
 */
ssize_t asgn1_read(struct file *, char __user *, size_t, loff_t *);
ssize_t asgn1_read(struct file *filp, char __user *buf, size_t count,
                   loff_t *f_pos)
{
  size_t size_read = 0;                   /* size read from virtual disk in this function */
  size_t begin_offset;                    /* the offset from the beginning of a page to
                           start reading */
  int begin_page_no = *f_pos / PAGE_SIZE; /* the first page which contains
               the requested data */
  int curr_page_no = 0;                   /* the current page number */
  size_t curr_size_read;                  /* size read from the virtual disk in this round */
  size_t size_to_be_read;                 /* size to be read in the current round in
                           while loop */

  struct list_head *ptr = asgn1_device.mem_list.next;
  page_node *curr;

  /* check f_pos, if beyond data_size, return 0 */
  if (*f_pos >= asgn1_device.data_size)
  {
    return 0;
  }

  /* adjust count if reading beyond data end */
  if (*f_pos + count > asgn1_device.data_size)
  {
    count = asgn1_device.data_size - *f_pos;
  }

  /* Traverse the list, once the first requested page is reached */
  list_for_each_entry(curr, &asgn1_device.mem_list, list)
  {
    if (curr_page_no >= begin_page_no)
    {
      void *page_addr;

      /* Calculate offset within page */
      if (curr_page_no == begin_page_no)
      {
        begin_offset = *f_pos % PAGE_SIZE;
      }
      else
      {
        begin_offset = 0;
      }

      /* Calculate size to read from this page */
      size_to_be_read = min(count - size_read, PAGE_SIZE - begin_offset);

      if (size_to_be_read <= 0)
        break;

      /* Map page and copy to user */
      page_addr = kmap_local_page(curr->page);
      curr_size_read = size_to_be_read - copy_to_user(buf + size_read,
                                                      page_addr + begin_offset,
                                                      size_to_be_read);
      kunmap_local(page_addr);

      if (curr_size_read < size_to_be_read)
      {
        if (size_read + curr_size_read == 0)
        {
          return -EINVAL; /* completely failed */
        }
        size_read += curr_size_read;
        break; /* partial copy, return what we got */
      }

      size_read += curr_size_read;

      /* if we've read everything requested, break */
      if (size_read >= count)
        break;
    }
    curr_page_no++;
  }

  *f_pos += size_read;
  return size_read;
}

static loff_t asgn1_lseek(struct file *, loff_t, int);
static loff_t asgn1_lseek(struct file *file, loff_t offset, int cmd)
{
  loff_t testpos;

  size_t buffer_size = asgn1_device.num_pages * PAGE_SIZE;

  /* set testpos according to the command */
  switch (cmd)
  {
  case SEEK_SET:
    testpos = offset;
    break;
  case SEEK_CUR:
    testpos = file->f_pos + offset;
    break;
  case SEEK_END:
    testpos = asgn1_device.data_size + offset;
    break;
  default:
    return -EINVAL;
  }

  /* if testpos larger than buffer_size, set testpos to buffer_size */
  if (testpos > buffer_size)
  {
    testpos = buffer_size;
  }

  /* if testpos smaller than 0, set testpos to 0 */
  if (testpos < 0)
  {
    testpos = 0;
  }

  /* set file->f_pos to testpos */
  file->f_pos = testpos;

  printk(KERN_INFO "Seeking to pos=%ld\n", (long)testpos);
  return testpos;
}

/**
 * This function writes from the user buffer to the virtual disk of this
 * module
 */
ssize_t asgn1_write(struct file *, const char __user *, size_t, loff_t *);
ssize_t asgn1_write(struct file *filp, const char __user *buf, size_t count,
                    loff_t *f_pos)
{
  size_t orig_f_pos = *f_pos;             /* the original file position */
  size_t size_written = 0;                /* size written to virtual disk in this function */
  size_t begin_offset;                    /* the offset from the beginning of a page to
                           start writing */
  int begin_page_no = *f_pos / PAGE_SIZE; /* the first page this finction
               should start writing to */

  int curr_page_no = 0;      /* the current page number */
  size_t curr_size_written;  /* size written to virtual disk in this round */
  size_t size_to_be_written; /* size to be read in the current round in
        while loop */

  ssize_t asgn1_write(struct file *, const char __user *, size_t, loff_t *);
  ssize_t asgn1_write(struct file * filp, const char __user *buf, size_t count,
                      loff_t *f_pos)
  {
    size_t orig_f_pos = *f_pos;             /* the original file position */
    size_t size_written = 0;                /* size written to virtual disk in this function */
    size_t begin_offset;                    /* the offset from the beginning of a page to
                             start writing */
    int begin_page_no = *f_pos / PAGE_SIZE; /* the first page this finction
                 should start writing to */

    int curr_page_no = 0;      /* the current page number */
    size_t curr_size_written;  /* size written to virtual disk in this round */
    size_t size_to_be_written; /* size to be read in the current round in
          while loop */

    struct list_head *ptr = asgn1_device.mem_list.next;
    page_node *curr;

    /* Traverse the list until the first page reached, and add nodes if necessary */
    while (curr_page_no < begin_page_no)
    {
      /* Check if we need to allocate more pages */
      if (curr_page_no >= asgn1_device.num_pages)
      {
        /* Allocate new page node */
        if (asgn1_device.cache)
        {
          curr = kmem_cache_alloc(asgn1_device.cache, GFP_KERNEL);
        }
        else
        {
          curr = kmalloc(sizeof(page_node), GFP_KERNEL);
        }

        if (!curr)
          return -ENOMEM;

        curr->page = alloc_page(GFP_KERNEL);
        if (!curr->page)
        {
          if (asgn1_device.cache)
          {
            kmem_cache_free(asgn1_device.cache, curr);
          }
          else
          {
            kfree(curr);
          }
          return -ENOMEM;
        }

        list_add_tail(&curr->list, &asgn1_device.mem_list);
        asgn1_device.num_pages++;
      }
      curr_page_no++;
    }

    /* Now write the data page by page */
    curr_page_no = 0;
    list_for_each_entry(curr, &asgn1_device.mem_list, list)
    {
      if (curr_page_no >= begin_page_no)
      {
        void *page_addr;

        /* Calculate offset within page */
        if (curr_page_no == begin_page_no)
        {
          begin_offset = *f_pos % PAGE_SIZE;
        }
        else
        {
          begin_offset = 0;
        }

        /* Calculate size to write to this page */
        size_to_be_written = min(count - size_written, PAGE_SIZE - begin_offset);

        if (size_to_be_written <= 0)
          break;

        /* Map page and copy from user */
        page_addr = kmap_local_page(curr->page);
        curr_size_written = size_to_be_written - copy_from_user(page_addr + begin_offset,
                                                                buf + size_written,
                                                                size_to_be_written);
        kunmap_local(page_addr);

        if (curr_size_written < size_to_be_written)
        {
          if (size_written + curr_size_written == 0)
          {
            return -EINVAL; /* completely failed */
          }
          size_written += curr_size_written;
          break; /* partial copy, return what we wrote */
        }

        size_written += curr_size_written;

        /* if we've written everything requested, break */
        if (size_written >= count)
          break;
      }
      curr_page_no++;

      /* If we need more pages, allocate them */
      if (curr_page_no >= asgn1_device.num_pages && size_written < count)
      {
        page_node *new_node;

        if (asgn1_device.cache)
        {
          new_node = kmem_cache_alloc(asgn1_device.cache, GFP_KERNEL);
        }
        else
        {
          new_node = kmalloc(sizeof(page_node), GFP_KERNEL);
        }

        if (!new_node)
          break;

        new_node->page = alloc_page(GFP_KERNEL);
        if (!new_node->page)
        {
          if (asgn1_device.cache)
          {
            kmem_cache_free(asgn1_device.cache, new_node);
          }
          else
          {
            kfree(new_node);
          }
          break;
        }

        list_add_tail(&new_node->list, &asgn1_device.mem_list);
        asgn1_device.num_pages++;
      }
    }

    *f_pos += size_written;
    asgn1_device.data_size = max(asgn1_device.data_size,
                                 orig_f_pos + size_written);
    return size_written;
  }
}

#define SET_NPROC_OP 1
#define TEM_SET_NPROC _IOW(MYIOC_TYPE, SET_NPROC_OP, int)

/**
 * The ioctl function, which nothing needs to be done in this case.
 */
long asgn1_ioctl(struct file *, unsigned, unsigned long);
long asgn1_ioctl(struct file *filp, unsigned cmd, unsigned long arg)
{
  int nr;
  int new_nprocs;
  int result;

  /* check whether cmd is for our device, if not for us, return -EINVAL */
  if (_IOC_TYPE(cmd) != MYIOC_TYPE)
  {
    return -EINVAL;
  }

  /* get command, and if command is SET_NPROC_OP, then get the data */
  nr = _IOC_NR(cmd);

  if (nr == SET_NPROC_OP)
  {
    if (copy_from_user(&new_nprocs, (int __user *)arg, sizeof(int)))
    {
      return -EFAULT;
    }

    /* check validity of the value before setting max_nprocs */
    if (new_nprocs <= 0)
    {
      return -EINVAL;
    }

    atomic_set(&asgn1_device.max_nprocs, new_nprocs);
    return 0;
  }

  return -ENOTTY;
}

static int asgn1_mmap(struct file *, struct vm_area_struct *);
static int asgn1_mmap(struct file *filp, struct vm_area_struct *vma)
{
  unsigned long pfn;
  unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
  unsigned long len = vma->vm_end - vma->vm_start;
  unsigned long ramdisk_size = asgn1_device.num_pages * PAGE_SIZE;
  page_node *curr;
  unsigned long index = 0;

  /* check offset and len */
  if (offset + len > ramdisk_size)
  {
    return -EINVAL;
  }

  /* loop through the entire page list, once the first requested page
     reached, add each page with remap_pfn_range one by one
     up to the last requested page */
  list_for_each_entry(curr, &asgn1_device.mem_list, list)
  {
    if (index >= vma->vm_pgoff)
    {
      if ((index - vma->vm_pgoff) * PAGE_SIZE >= len)
      {
        break;
      }

      pfn = page_to_pfn(curr->page);
      if (remap_pfn_range(vma,
                          vma->vm_start + (index - vma->vm_pgoff) * PAGE_SIZE,
                          pfn,
                          PAGE_SIZE,
                          vma->vm_page_prot))
      {
        return -EAGAIN;
      }
    }
    index++;
  }

  return 0;
}

struct file_operations asgn1_fops = {
    .owner = THIS_MODULE,
    .read = asgn1_read,
    .write = asgn1_write,
    .unlocked_ioctl = asgn1_ioctl,
    .open = asgn1_open,
    .mmap = asgn1_mmap,
    .release = asgn1_release,
    .llseek = asgn1_lseek};

static void *my_seq_start(struct seq_file *s, loff_t *pos)
{
  if (*pos >= 1)
    return NULL;
  else
    return &asgn1_dev_count + *pos;
}

static void *my_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
  (*pos)++;
  if (*pos >= 1)
    return NULL;
  else
    return &asgn1_dev_count + *pos;
}

static void my_seq_stop(struct seq_file *s, void *v)
{
  /* There's nothing to do here! */
}

int my_seq_show(struct seq_file *, void *);
int my_seq_show(struct seq_file *s, void *v)
{
  /* use seq_printf to print some info to s */
  seq_printf(s, "Device: %s\n", MYDEV_NAME);
  seq_printf(s, "Major: %d\n", MAJOR(asgn1_device.dev));
  seq_printf(s, "Minor: %d\n", MINOR(asgn1_device.dev));
  seq_printf(s, "Number of pages: %d\n", asgn1_device.num_pages);
  seq_printf(s, "Data size: %zu bytes\n", asgn1_device.data_size);
  seq_printf(s, "Current processes: %d\n", atomic_read(&asgn1_device.nprocs));
  seq_printf(s, "Max processes: %d\n", atomic_read(&asgn1_device.max_nprocs));
  return 0;
}

static struct seq_operations my_seq_ops = {
    .start = my_seq_start,
    .next = my_seq_next,
    .stop = my_seq_stop,
    .show = my_seq_show};

static int my_proc_open(struct inode *inode, struct file *filp)
{
  return seq_open(filp, &my_seq_ops);
}

static struct proc_ops asgn1_proc_ops = {
    .proc_open = my_proc_open,
    .proc_lseek = seq_lseek,
    .proc_read = seq_read,
    .proc_release = seq_release,
};

/**
 * Initialise the module and create the master device
 */
int __init asgn1_init_module(void);
int __init asgn1_init_module(void)
{
  int result;

  /* set nprocs and max_nprocs of the device */
  atomic_set(&asgn1_device.nprocs, 0);
  atomic_set(&asgn1_device.max_nprocs, 1);

  /* allocate major number */
  result = alloc_chrdev_region(&asgn1_device.dev, asgn1_minor,
                               asgn1_dev_count, MYDEV_NAME);
  if (result < 0)
  {
    printk(KERN_WARNING "%s: can't get major number\n", MYDEV_NAME);
    goto fail_device;
  }
  asgn1_major = MAJOR(asgn1_device.dev);

  /* allocate cdev, and set ops and owner field */
  asgn1_device.cdev = cdev_alloc();
  if (!asgn1_device.cdev)
  {
    printk(KERN_WARNING "%s: can't alloc cdev\n", MYDEV_NAME);
    result = -ENOMEM;
    goto fail_malloc;
  }

  asgn1_device.cdev->ops = &asgn1_fops;
  asgn1_device.cdev->owner = THIS_MODULE;

  /* add cdev */
  result = cdev_add(asgn1_device.cdev, asgn1_device.dev, asgn1_dev_count);
  if (result)
  {
    printk(KERN_WARNING "%s: can't add cdev\n", MYDEV_NAME);
    goto fail_cdev;
  }

  /* initialize the page list */
  INIT_LIST_HEAD(&asgn1_device.mem_list);
  asgn1_device.num_pages = 0;
  asgn1_device.data_size = 0;

  /* create proc entries */
  asgn1_device.cache = kmem_cache_create("asgn1_cache",
                                         sizeof(page_node),
                                         0,
                                         SLAB_HWCACHE_ALIGN,
                                         NULL);
  if (!asgn1_device.cache)
  {
    printk(KERN_WARNING "%s: can't create cache\n", MYDEV_NAME);
    result = -ENOMEM;
    goto fail_kmem_cache;
  }

  proc_create(MYDEV_NAME, 0, NULL, &asgn1_proc_ops);

  asgn1_device.class = class_create(MYDEV_NAME);
  if (IS_ERR(asgn1_device.class))
  {
    printk(KERN_WARNING "%s: can't create class\n", MYDEV_NAME);
    result = PTR_ERR(asgn1_device.class);
    goto fail_class;
  }

  asgn1_device.device = device_create(asgn1_device.class, NULL,
                                      asgn1_device.dev, NULL, "%s", MYDEV_NAME);
  if (IS_ERR(asgn1_device.device))
  {
    printk(KERN_WARNING "%s: can't create udev device\n", MYDEV_NAME);
    result = PTR_ERR(asgn1_device.device);
    goto fail_device_create;
  }

  printk(KERN_WARNING "set up udev entry\n");
  printk(KERN_WARNING "Hello world from %s\n", MYDEV_NAME);
  return 0;

  /* cleanup code called when any of the initialization steps fail */
fail_device_create:
  class_destroy(asgn1_device.class);
fail_class:
  remove_proc_entry(MYDEV_NAME, NULL);
  if (asgn1_device.cache)
  {
    kmem_cache_destroy(asgn1_device.cache);
  }
fail_kmem_cache:
  cdev_del(asgn1_device.cdev);
fail_cdev:
  kfree(asgn1_device.cdev);
fail_malloc:
  unregister_chrdev_region(asgn1_device.dev, asgn1_dev_count);
fail_device:

  return result;
}

/**
 * Finalise the module
 */
void __exit asgn1_exit_module(void);
void __exit asgn1_exit_module(void)
{
  device_destroy(asgn1_device.class, asgn1_device.dev);
  class_destroy(asgn1_device.class);
  printk(KERN_WARNING "cleaned up udev entry\n");

  /* free all pages in the page list */
  free_memory_pages();

  /* cleanup in reverse order */
  remove_proc_entry(MYDEV_NAME, NULL);
  if (asgn1_device.cache)
  {
    kmem_cache_destroy(asgn1_device.cache);
  }
  cdev_del(asgn1_device.cdev);
  kfree(asgn1_device.cdev);
  unregister_chrdev_region(asgn1_device.dev, asgn1_dev_count);

  printk(KERN_WARNING "Good bye from %s\n", MYDEV_NAME);
}

module_init(asgn1_init_module);
module_exit(asgn1_exit_module);
