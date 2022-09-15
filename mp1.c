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

static struct list_head __rcu task_list_head = LIST_HEAD_INIT(task_list_head);

// Workqueue

// Locks
static DEFINE_SPINLOCK(ll_mutex);

static ssize_t mp1_proc_read_callback(struct file *file, char __user *buffer, size_t count, loff_t *off) {
   // implementation goes here...
   struct process_list_node *entry;
   size_t buf_size = count + 1;
   size_t to_copy_to_user = 0;
   ssize_t copied = 0;

   char *kernel_buf = (char *) kzalloc(buf_size, GFP_KERNEL);
   
   list_for_each_entry(entry, &task_list_head, list) {
      to_copy_to_user += snprintf(kernel_buf + to_copy_to_user, buf_size - to_copy_to_user, "%d: %ld\n", entry->pid, entry->cpu_use);
   }

   copied += simple_read_from_buffer(buffer, count, off, kernel_buf, to_copy_to_user);
   printk(PREFIX"read(%zu) copied %lu bytes to userspace address\n", count, copied);

   kfree(kernel_buf);

   return copied;
}

static ssize_t mp1_proc_write_callback(struct file *file, const char __user *buffer, size_t count, loff_t *off) {
   struct process_list_node* new;
   unsigned long cpu_use = 0;
   ssize_t copied = 0;
   int pid = 0;
   size_t kernel_buf_size = count + 1;
   char *kernel_buf = (char *) kzalloc(kernel_buf_size, GFP_KERNEL);

   copied += simple_write_to_buffer(kernel_buf, kernel_buf_size, off, buffer, count);

   if ( sscanf(kernel_buf, "%d", &pid) == 1 ) {
      printk(PREFIX"Found pid=%d\n", pid);

      if ( get_cpu_use(pid, &cpu_use) != -1 ) {
         new = kmalloc(sizeof(struct process_list_node), GFP_KERNEL);
         new->pid = pid;
         new->cpu_use = cpu_use;
         list_add(&new->list, task_list_head.next);
         list_add_rcu()
      } else {
         printk(PREFIX"pid=%d is not a valid pid\n", pid);
      }
   } else {
      printk(PREFIX"Failed to parse pid from buffer\n");
   }

   return copied;
}

void update_registered_process_cpu_time(void* ptr) {
   struct process_list_node *process_node;
   struct list_head *pos, *n;

   list_for_each_safe(pos, n, &task_list_head) {
      process_node = list_entry(pos, struct process_list_node, list);

      // try to get cpu use, if process isn't valid, remove from list
      if ( get_cpu_use(process_node->pid, &process_node->cpu_use) == -1 ) {
         printk(PREFIX"pid %d is no longer valid, removing from list\n", process_node->pid);
         list_del(pos);
         kfree(process_node);
      }
   };
}

void timer_callback(struct timer_list *data) {
   struct process_list_node *entry;

   printk(PREFIX"Timer callback function called\n");
   
   list_for_each_entry(entry, &task_list_head, list) {
      printk(PREFIX"pid: %d\n", entry->pid);
   }

   mod_timer(&mp1_timer, jiffies + msecs_to_jiffies(TIMEOUT));
}

static const struct proc_ops mp1_file_ops = {
   .proc_read = mp1_proc_read_callback,
   .proc_write = mp1_proc_write_callback,
};

// mp1_init - Called when module is loaded
int __init mp1_init(void) {
   #ifdef DEBUG
   printk(KERN_ALERT "MP1 MODULE LOADING\n");
   #endif
   
   proc_dir = proc_mkdir(DIRECTORY, NULL);
   proc_entry = proc_create(FILENAME, 0666, proc_dir, &mp1_file_ops);

   timer_setup(&mp1_timer, timer_callback, 0);
   mod_timer(&mp1_timer, jiffies + msecs_to_jiffies(TIMEOUT));
   
   printk(KERN_ALERT "MP1 MODULE LOADED\n");
   return 0;
}

// mp1_exit - Called when module is unloaded
void __exit mp1_exit(void) {
   struct process_list_node *entry, *tmp;

   #ifdef DEBUG
   printk(KERN_ALERT "MP1 MODULE UNLOADING\n");
   #endif

   // Remove the proc fs entry
   remove_proc_entry(FILENAME, proc_dir);
   remove_proc_entry(DIRECTORY, NULL);

   // Delete the timer
   del_timer(&mp1_timer);

   // Delete all entries in the linked list
   list_for_each_entry_safe(entry, tmp, &task_list_head, list) {
      printk(PREFIX"freeing pid %d\n", entry->pid);
      list_del(&entry->list);
      kfree(entry);
   };

   printk(KERN_ALERT "MP1 MODULE UNLOADED\n");
}

// Register init and exit funtions
module_init(mp1_init);
module_exit(mp1_exit);
