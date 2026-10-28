#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/bitops.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("CAU SYSLAB");
MODULE_DESCRIPTION("Assignment 6 - Test and Set Module");

#define NUM_THREADS 4

static struct task_struct *threads[NUM_THREADS];
static unsigned long lock_flag = 0; 
static int counter = 0;             

static int test_and_set_function(void *data)
{
    while (!kthread_should_stop()) {
        int val;

        while (test_and_set_bit(0, &lock_flag)) {
            cpu_relax();
        }

        val = ++counter;

        clear_bit(0, &lock_flag);

        printk(KERN_INFO "pid [%d] test_and_set_function: counter: %d\n", current->pid, val);

        msleep(500);
    }
    return 0;
}

static int __init test_and_set_module_init(void)
{
    int i;
    printk(KERN_INFO "test_and_set_module_init: Entering Test and Set Module!\n");

    for (i = 0; i < NUM_THREADS; i++) {
        threads[i] = kthread_run(test_and_set_function, NULL, "tas_thread_%d", i);
        if (IS_ERR(threads[i])) {
            printk(KERN_ERR "Failed to create thread %d\n", i);
            return PTR_ERR(threads[i]);
        }
    }

    return 0;
}

static void __exit test_and_set_module_cleanup(void)
{
    int i;

    for (i = 0; i < NUM_THREADS; i++) {
        if (threads[i]) {
            kthread_stop(threads[i]);
        }
    }

    printk(KERN_INFO "test_and_set_module_cleanup: Exiting Test and Set Module!\n");
}

module_init(test_and_set_module_init);
module_exit(test_and_set_module_cleanup);
