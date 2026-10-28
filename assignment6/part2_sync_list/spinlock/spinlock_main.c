#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/delay.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("CAU SYSLAB");
MODULE_DESCRIPTION("Assignment 6 - Spinlock Linked List Module");

#define NUM_THREADS 4
#define TOTAL_NODES 1000000
#define NODES_PER_THREAD (TOTAL_NODES / NUM_THREADS)

static struct task_struct *threads[NUM_THREADS];
static int thread_ids[NUM_THREADS];

// linked_list_impl.c에 정의된 함수 및 변수 참조
extern void *add_to_list(int thread_id, int range_bound[]);
extern int search_list(int thread_id, void *data, int range_bound[]);
extern int delete_from_list(int thread_id, int range_bound[]);
extern unsigned long long insert_time, insert_count;
extern unsigned long long search_time, search_count;
extern unsigned long long delete_time, delete_count;

static void set_iter_range(int thread_id, int range_bound[])
{
    range_bound[0] = (thread_id - 1) * NODES_PER_THREAD;
    range_bound[1] = thread_id * NODES_PER_THREAD - 1;
}

static int work_fn(void *data)
{
    int range_bound[2];
    int thread_id = *(int *)data;
    void *ret;

    set_iter_range(thread_id, range_bound);

    ret = add_to_list(thread_id, range_bound);
    search_list(thread_id, ret, range_bound);
    delete_from_list(thread_id, range_bound);

    while (!kthread_should_stop()) {
        msleep(500);
    }

    printk(KERN_INFO "thread #%d stopped!\n", thread_id);
    return 0;
}

static int __init spinlock_module_init(void)
{
    int i;
    printk(KERN_INFO "spinlock_module_init: Entering Spinlock Module!\n");

    for (i = 0; i < NUM_THREADS; i++) {
        thread_ids[i] = i + 1;
        threads[i] = kthread_run(work_fn, &thread_ids[i], "spin_thread_%d", i + 1);
        if (IS_ERR(threads[i])) {
            printk(KERN_ERR "Failed to create thread %d\n", i + 1);
            return PTR_ERR(threads[i]);
        }
    }

    return 0;
}

static void __exit spinlock_module_cleanup(void)
{
    int i;

    // 슬라이드 38 로그 포맷: 측정 시간 및 카운트 출력
    printk(KERN_INFO "spinlock_module_cleanup: Spinlock linked list insert time: %llu ns, count: %llu\n", insert_time, insert_count);
    printk(KERN_INFO "spinlock_module_cleanup: Spinlock linked list search time: %llu ns, count: %llu\n", search_time, search_count);
    printk(KERN_INFO "spinlock_module_cleanup: Spinlock linked list delete time: %llu ns, count: %llu\n", delete_time, delete_count);

    for (i = 0; i < NUM_THREADS; i++) {
        if (threads[i]) {
            kthread_stop(threads[i]);
        }
    }

    printk(KERN_INFO "spinlock_module_cleanup: Exiting Spinlock Module!\n");
}

module_init(spinlock_module_init);
module_exit(spinlock_module_cleanup);
