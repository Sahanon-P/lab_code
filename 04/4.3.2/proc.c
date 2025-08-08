/*
* Using the /proc filesystem. (/proc/driver solution)
*
* Write a module that creates a /proc filesystem entry and can read
* and write to it.
*
* When you read from the entry, you should obtain the value of some
* parameter set in your module.
*
* When you write to the entry, you should modify that value, which
* should then be reflected in a subsequent read.
*
* Make sure you remove the entry when you unload your module.  What
* happens if you don't and you try to access the entry after the
* module has been removed?
*/

#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/init.h>
#include <linux/slab.h>

#if 0
#define NODE "my_proc"
#else
#define NODE "driver/my_proc"
#endif

static struct proc_dir_entry *my_proc_dir;

#define MODULE_NAME "proc_test"
#define MAXBUFLEN 100
static char msg[MAXBUFLEN+1]="777\n";
static int dlen=4, wptr=0, rptr=0;
static int param=777;

static int
my_proc_open(struct inode *inode, struct file *filp)
{
rptr = 0;
wptr = 0;

  // if not opened in read-only mode, clean message buffer
  if ((filp->f_flags & O_ACCMODE) != O_RDONLY){
    memset(msg, 0, MAXBUFLEN+1);
    dlen = 0;
  }
return 0;
}

static int
my_proc_read(struct file *filp,char *buf,size_t count,loff_t *offp )
{
int rc;
	memset(msg, 0, MAXBUFLEN+1);
	sprintf(msg, "param = %d\n", param);
	dlen = strlen(msg);
	if(rptr >= dlen) return 0;
	if(count>dlen-rptr) count=dlen-rptr;
	rc = copy_to_user(buf+rptr, msg, count);
	rptr = rptr + count - rc;
	return count-rc;
}


static int
my_proc_write(struct file *filp,const char *buf,size_t count,loff_t *offp)
{
int rc;
	if(wptr >= MAXBUFLEN) return 0;
	if(count > MAXBUFLEN - wptr) count = MAXBUFLEN - wptr;
	rc = copy_from_user(msg+wptr, buf, count);
	dlen = dlen + count - rc;
	wptr = wptr + count - rc;
	return count - rc;
}

int my_proc_close(struct inode *inode, struct file *filp)
{
  // if not opened in read-only mode, process the message buffer
  if ((filp->f_flags & O_ACCMODE) != O_RDONLY){
    //parse_msg(msg); FILL ME HERE!!! Make sure there is sanity check!
  }
return 0;
}


struct proc_ops my_proc_ops = {
.proc_open = my_proc_open,
.proc_read = my_proc_read,
.proc_write = my_proc_write,
.proc_release = my_proc_close,
};


static int __init my_init(void)
{
	/**
	 * create a proc entry with global read access and owner write access
         *  if fails, print an error message and return an error.
	 *
	 */
	my_proc_dir = proc_mkdir(NODE, NULL);
	if(my_proc_dir == NULL) {
		printk(KERN_WARNING "mod_test_proc: can't add %s to the procfs\n", NODE);
		return -ENOMEM;
	}

	if(proc_create("rw_info", S_IRUGO|S_IWUSR, my_proc_dir, &my_proc_ops)==NULL){
		remove_proc_entry(NODE, NULL);
		printk(KERN_WARNING "mod_test_proc: can't add %s/rw_info to the procfs\n", NODE);
		return -ENOMEM;
	}

	printk(KERN_WARNING "mod_test_proc successful\n");
	return 0;
}

static void __exit my_exit(void)
{
	/**
	 * remove the proc entry
	 */
	remove_proc_entry("rw_info", my_proc_dir);
	remove_proc_entry(NODE, NULL);
}

module_init(my_init);
module_exit(my_exit);

MODULE_AUTHOR("Zhiyi Huang");
MODULE_DESCRIPTION("Create a readable and writable proc entry in Lab 4 of COSC440");
MODULE_LICENSE("GPL v2");
