/* **************** LDD:1.0 s_07/lab1_list.c **************** */
/*
 * The code herein is: Copyright Jerry Cooperstein, 2009
 *
 * This Copyright is retained for the purpose of protecting free
 * redistribution of source.
 *
 *     URL:    http://www.coopj.com
 *     email:  coop@coopj.com
 *
 * The primary maintainer for this code is Jerry Cooperstein
 * The CONTRIBUTORS file (distributed with this
 * file) lists those known to have contributed to the source.
 *
 * This code is distributed under Version 2 of the GNU General Public
 * License, which you should have received with the source.
 *
 */
/* Linked lists
 *
 * Write a module that sets up a doubly-linked circular list of data
 * structures.  The data structure can be as simple as an integer
 * variable.
 *
 * Test inserting and deleting elements in the list.
 *
 * Walk through the list (using list_entry()) and print out values to
 * make sure the insertion and deletion processes are working.
 @*/

#include <linux/module.h>
#include <asm/atomic.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/init.h>

static LIST_HEAD(my_list);

struct my_entry {
	struct list_head clist;	
	int val;
};

static int __init my_init(void)
{
	struct my_entry *ce;
	int k;

	/* COMPLETE ME
	 *
	 * add five nodes to my_list, and set the val of each node
	 *
	 * traverse list ce and print value of each node
	 */

	return 0;
}

static void __exit my_exit(void)
{
	struct my_entry *list;	/* pointer to list head object */
	struct my_entry *tmp;	/* temporary list head for safe deletion */

	/* COMPLETE ME
	 *
	 * delete all the nodes from my_list
	 *
	 * check whether my_list is empty
	 */

}

module_init(my_init);
module_exit(my_exit);

MODULE_AUTHOR("Dave Harris");
/* many modifications by Jerry Cooperstein */
MODULE_DESCRIPTION("LDD:1.0 s_07/lab1_list.c");
MODULE_LICENSE("GPL v2");
