#define LINUX
#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/workqueue.h>
#include <linux/proc_fs.h>
#include <linux/timer.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/pid.h>
#include <linux/sched.h>

#define find_task_by_pid(nr) pid_task(find_vpid(nr), PIDTYPE_PID)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("neilk3");
MODULE_DESCRIPTION("CS-423 MP1");

#define DEBUG 1
#define FILENAME "status"
#define DIRECTORY "mp1"
#define TIMEOUT 5000
#define BASE_10 10

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define PREFIX "[MP1] "

// This struct represents a process registered with this kernel module
struct process_list_node {
   struct list_head list; // the link node in our linked list
   int pid;               // the pid of the registered application
   unsigned long cpu_use; // the cpu usage of the application (updated every 5s)
};

// This is the head of our list of tasks to monitor. 
// Keep the head initialized to keep things simple.
static struct list_head task_list_head = LIST_HEAD_INIT(task_list_head);

// RW lock for synchronizing reads & writes to our list of processes to monitor.
static DEFINE_SPINLOCK(rp_lock);

// This proc entry represents the directory mp1 in the procfs 
static struct proc_dir_entry *proc_dir;

// This timer fires off every 5 seconds & schedules some work in our workqueue.
static struct timer_list work_timer;

// Define our own workqueue for top/bottom half updates. 
// This workqueue will be define with only 1 kernel thread.
static struct workqueue_struct *workqueue;

// A callback function run on the worker thread associated with the workqueue.
static void update_cpu_time_fn(struct work_struct *work); // this work function is implemented later on

// Initialize the work struct that will schedule the callback function to run on 
// this module's workqueue worker thread.
static DECLARE_WORK(update_cpu_time_work, update_cpu_time_fn);

// This function returns 0 if the PID is valid and the CPU time was successfully 
// returned by the parameter cpu_use. Otherwise, it returns -1
int get_cpu_use(int pid, unsigned long *cpu_use)
{
   struct task_struct* task;
   rcu_read_lock();
   task=find_task_by_pid(pid);
   if (task!=NULL)
   {  
	*cpu_use=task->utime;
        rcu_read_unlock();
        return 0;
   }
   else 
   {
     rcu_read_unlock();
     return -1;
   }
}

static void update_cpu_time_fn(struct work_struct *work) {
   struct process_list_node *entry, *tmp;

   // CRITICAL SECTION: either update or delete each process node
   spin_lock(&rp_lock);
   list_for_each_entry_safe(entry, tmp, &task_list_head, list) {
      // try to get cpu use, if process isn't valid, remove from list
      if ( get_cpu_use(entry->pid, &entry->cpu_use) == -1 ) {
         printk(PREFIX"pid %d is no longer valid, removing from list\n", entry->pid);
         list_del(&entry->list);
         kfree(entry);
      }
   }
   spin_unlock(&rp_lock);
}

// This callback is run every 5 seconds when the timer fires.
// It is the top half that schedules work to be run on the kernel worker thread.
void timer_callback(struct timer_list *data) {
   queue_work(workqueue, &update_cpu_time_work);
   mod_timer(&work_timer, jiffies + msecs_to_jiffies(TIMEOUT));
}

// This callback copies each registered process & its cpu time into the provided 
// user-space buffer when a read is requested from our procfs entry. 
// NOTE: holds a read lock
static ssize_t mp1_proc_read_callback(
      struct file *file, char __user *buffer, size_t count, loff_t *off) {
   struct process_list_node *entry;
   size_t buf_size = count + 1;
   size_t to_copy = 0;
   ssize_t copied = 0;

   char *kernel_buf = (char *) kzalloc(buf_size, GFP_KERNEL);
   
   // Go through each entry of the list and read + format the pid and cpu use
   spin_lock(&rp_lock);
   list_for_each_entry(entry, &task_list_head, list) {
      to_copy += 
         snprintf(kernel_buf + to_copy, buf_size - to_copy, 
            "%d: %ld\n", entry->pid, entry->cpu_use);
   }
   spin_unlock(&rp_lock);

   copied += simple_read_from_buffer(buffer, count, off, kernel_buf, to_copy);
   kfree(kernel_buf);

   return copied;
}

// This function initializes a node in our list of registered processes and adds 
// it to our linked list. NOTE: holds a write lock.
void register_process(int pid, unsigned long current_cpu_use) {
   struct process_list_node* new = kmalloc(sizeof(struct process_list_node), GFP_KERNEL);
   new->pid = pid;
   new->cpu_use = current_cpu_use;

   // CRITICAL SECTION: insert a node into our list of processes
   spin_lock(&rp_lock);
   list_add(&new->list, task_list_head.next);
   spin_unlock(&rp_lock);
}

// This callback attemps to parse the content written by the user to a pid. This
// function will register the pid and the cpu time if the pid is valid. 
// NOTE: this function calls another function that holds a write lock.
static ssize_t mp1_proc_write_callback(
      struct file *file, const char __user *buffer, size_t count, loff_t *off) {
   unsigned long cpu_use = 0;
   int pid, parse_result;
   ssize_t copied = 0;
   size_t kernel_buf_size = count + 1;
   char *kernel_buf = (char *) kzalloc(kernel_buf_size, GFP_KERNEL);

   // Copy the userspace memory into our kernel buffer
   copied += simple_write_to_buffer(kernel_buf, kernel_buf_size, off, buffer, count);

   // parse_result = (sscanf(kernel_buf, "%d", &pid) == 1);
   parse_result = ( kstrtoint(kernel_buf, BASE_10, &pid ) == 0 );
   if ( parse_result ) {
      printk(PREFIX"Found pid=%d\n", pid);

      // Try and get cpu_use, if we can register the process
      if ( get_cpu_use(pid, &cpu_use) != -1 ) {
         register_process(pid, cpu_use);
      } else { // if we failed to get cpu use, do not register the process
         printk(PREFIX"pid=%d is not a valid pid\n", pid);
      }
   } else { // if we cannot parse the buffer to an integer, do not register
      printk(PREFIX"Failed to parse pid from buffer\n");
   }

   return copied;
}

// This struct contains callbacks for operations on our procfs entry.
static const struct proc_ops mp1_file_ops = {
   .proc_read = mp1_proc_read_callback,
   .proc_write = mp1_proc_write_callback,
};

// mp1_init - Called when module is loaded
int __init mp1_init(void) {
   #ifdef DEBUG
   printk(KERN_ALERT "MP1 MODULE LOADING\n");
   #endif
   
   // Setup proc fs entry
   proc_dir = proc_mkdir(DIRECTORY, NULL);
   proc_create(FILENAME, 0666, proc_dir, &mp1_file_ops);

   // Setup timer
   timer_setup(&work_timer, timer_callback, 0);
   mod_timer(&work_timer, jiffies + msecs_to_jiffies(TIMEOUT));

   // Setup workqueue
   workqueue = create_singlethread_workqueue("mp1-worker");
   
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
   del_timer(&work_timer);

   // Flush all work and destroy the workqueue
   flush_workqueue(workqueue);
   destroy_workqueue(workqueue);

   // Delete all entries in the linked list
   spin_lock(&rp_lock);
   list_for_each_entry_safe(entry, tmp, &task_list_head, list) {
      printk(PREFIX"removing process with pid %d\n", entry->pid);
      list_del(&entry->list);
      kfree(entry);
   };
   spin_unlock(&rp_lock);

   printk(KERN_ALERT "MP1 MODULE UNLOADED\n");
}

// Register init and exit funtions
module_init(mp1_init);
module_exit(mp1_exit);
