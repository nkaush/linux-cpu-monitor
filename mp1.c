#define LINUX
#include "mp1_given.h"

#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/workqueue.h>
#include <linux/proc_fs.h>
#include <linux/timer.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/slab.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("neilk3");
MODULE_DESCRIPTION("CS-423 MP1");

#define DEBUG 1
#define FILENAME "status"
#define DIRECTORY "mp1"
#define TIMEOUT 5000

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define PREFIX "[MP1] "

// Proc FS entries
static struct proc_dir_entry *proc_dir;
static struct proc_dir_entry *proc_entry;

// Timer
static struct timer_list mp1_timer;

// List of Processes
struct process_list_node {
   struct list_head list; /* kernel's list structure */
   int pid;
   unsigned long cpu_use;
};

static struct list_head task_list_head = LIST_HEAD_INIT(task_list_head);

// Workqueue

// Locks

static ssize_t mp1_read(struct file *file, char __user *buffer, size_t count, loff_t *off) {
   // implementation goes here...
   size_t buf_size = count + 1;
   char *buf = (char *) kmalloc(buf_size, GFP_KERNEL);
   ssize_t copied = 0;

   size_t to_copy = MIN(buf_size, 7);
   snprintf(buf, to_copy, "hello\n");

   copied += simple_read_from_buffer(buffer, count, off, buf, to_copy);

   printk(PREFIX"read(%zu) copied %lu bytes to userspace address\n", count, copied);

   kfree(buf);

   return copied;
}

static ssize_t mp1_write(struct file *file, const char __user *buffer, size_t count, loff_t *off) {
   // implementation goes here...
   // struct process_list_node* new = kmalloc(sizeof(struct process_list_node), GFP_KERNEL);
   // list_add_tail(&new->list, task_list_head.next);
   size_t kernel_buf_size = count + 1;
   char *kernel_buf = (char *) kmalloc(kernel_buf_size, GFP_KERNEL);
   ssize_t copied = 0;

   copied += simple_write_to_buffer(kernel_buf, kernel_buf_size, off, buffer, count);

   int pid = 0;
   if ( sscanf(kernel_buf, "%d", &pid) == 1 ) {
      printk(PREFIX"Found pid=%d\n", pid);
   } else {
      printk(PREFIX"Failed to parse pid from buffer\n");
   }

   return copied;
}

static const struct proc_ops mp1_file = {
   .proc_read = mp1_read,
   .proc_write = mp1_write,
};

void mp1_timer_callback(struct timer_list * data) {
   printk(PREFIX"Timer callback function called\n");
    
   mod_timer(&mp1_timer, jiffies + msecs_to_jiffies(TIMEOUT));

   // Begin list iteration
   // struct list_head *ptr = NULL;
   // struct process_list_node *entry;

   // for (ptr = task_list_head.next; ptr != &task_list_head; ptr = ptr->next) {
   //    entry = list_entry(ptr, struct process_list_node, list);
   //    // list_add_tail(&new->list, ptr); // where new == struct todo_struct *
   // }
   // End list iteration
}

// mp1_init - Called when module is loaded
int __init mp1_init(void) {
   #ifdef DEBUG
   printk(KERN_ALERT "MP1 MODULE LOADING\n");
   #endif
   // Insert your code here ...
   
   proc_dir = proc_mkdir(DIRECTORY, NULL);
   proc_entry = proc_create(FILENAME, 0666, proc_dir, &mp1_file);

   timer_setup(&mp1_timer, mp1_timer_callback, 0);
   mod_timer(&mp1_timer, jiffies + msecs_to_jiffies(TIMEOUT));
   
   printk(KERN_ALERT "MP1 MODULE LOADED\n");
   return 0;
}

// mp1_exit - Called when module is unloaded
void __exit mp1_exit(void) {
   #ifdef DEBUG
   printk(KERN_ALERT "MP1 MODULE UNLOADING\n");
   #endif
   // Insert your code here ...
   
   // Remove the proc fs entry
   remove_proc_entry(FILENAME, proc_dir);
   remove_proc_entry(DIRECTORY, NULL);

   // Delete the timer
   del_timer(&mp1_timer);

   // Delete all entries in the linked list
   // struct process_list_node *tmp;
   // struct list_head *pos, *q;

   // list_for_each_safe(pos, q, &task_list_head) {
   //    tmp = list_entry(pos, struct process_list_node, list);
   //    printk("freeing pid %d\n", tmp->pid);
   //    list_del(pos);
   //    kfree(tmp);
   // };

   printk(KERN_ALERT "MP1 MODULE UNLOADED\n");
}

// Register init and exit funtions
module_init(mp1_init);
module_exit(mp1_exit);
