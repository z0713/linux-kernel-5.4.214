#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/delay.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("CAU SYSLAB");
MODULE_DESCRIPTION("Assignment 6 - Compare and Swap Module");

#define NUM_THREADS 4

static struct task_struct *threads[NUM_THREADS];
static int counter = 0; 

static int compare_and_swap_function(void *data)
{
    while (!kthread_should_stop()) {
        int old_val;
        int new_val;

        do {
            old_val = counter;
            new_val = old_val + 1;
        } while (cmpxchg(&counter, old_val, new_val) != old_val);

        printk(KERN_INFO "pid [%d] compare_and_swap_function: counter: %d\n", current->pid, new_val);

        msleep(500);
    }
    return 0;
}

static int __init compare_and_swap_module_init(void)
{
    int i;
    printk(KERN_INFO "compare_and_swap_module_init: Entering Compare and swap Module!\n");

    for (i = 0; i < NUM_THREADS; i++) {
        threads[i] = kthread_run(compare_and_swap_function, NULL, "cas_thread_%d", i);
        if (IS_ERR(threads[i])) {
            printk(KERN_ERR "Failed to create thread %d\n", i);
            return PTR_ERR(threads[i]);
        }
    }

    return 0;
}

static void __exit compare_and_swap_module_cleanup(void)
{
    int i;

    for (i = 0; i < NUM_THREADS; i++) {
        if (threads[i]) {
            kthread_stop(threads[i]);
        }
    }

    printk(KERN_INFO "compare_and_swap_module_cleanup: Exiting Compare and Swap Module!\n");
}

module_init(compare_and_swap_module_init);
module_exit(compare_and_swap_module_cleanup);
