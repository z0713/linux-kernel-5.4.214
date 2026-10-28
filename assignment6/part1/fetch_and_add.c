#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/atomic.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("CAU SYSLAB");
MODULE_DESCRIPTION("Assignment 6 - Fetch and Add Module");

#define NUM_THREADS 4

static struct task_struct *threads[NUM_THREADS];
static atomic_t counter = ATOMIC_INIT(0); // 원자적 카운터 선언 및 0으로 초기화

static int fetch_and_add_function(void *data)
{
    while (!kthread_should_stop()) {
        // 원자적으로 1 증가시키고 그 결과값을 반환받음 (Fetch-and-Add)
        int val = atomic_inc_return(&counter);

        printk(KERN_INFO "pid [%d] fetch_and_add_function: counter: %d\n", current->pid, val);

        msleep(500);
    }
    return 0;
}

static int __init fetch_and_add_module_init(void)
{
    int i;
    printk(KERN_INFO "fetch_and_add_module_init: Entering Fetch and Add Module!\n");

    for (i = 0; i < NUM_THREADS; i++) {
        threads[i] = kthread_run(fetch_and_add_function, NULL, "faa_thread_%d", i);
        if (IS_ERR(threads[i])) {
            printk(KERN_ERR "Failed to create thread %d\n", i);
            return PTR_ERR(threads[i]);
        }
    }

    return 0;
}

static void __exit fetch_and_add_module_cleanup(void)
{
    int i;

    for (i = 0; i < NUM_THREADS; i++) {
        if (threads[i]) {
            kthread_stop(threads[i]);
        }
    }

    printk(KERN_INFO "fetch_and_add_module_cleanup: Exiting Fetch and Add Module!\n");
}

module_init(fetch_and_add_module_init);
module_exit(fetch_and_add_module_cleanup);
