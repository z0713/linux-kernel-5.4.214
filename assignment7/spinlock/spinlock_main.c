#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/errno.h>
#include "calclock.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("CAU SYSLAB");
MODULE_DESCRIPTION("Assignment 7 - Spinlock Linked List Module");

#define NUM_THREADS 4
#define TOTAL_NODES 1000000

/* 슬라이드 25 구조체 */
struct animal {
    int total;
    struct list_head entry;
    spinlock_t lock;
};

struct cat {
    int var;
    struct list_head entry;
};

static struct animal *head;
static struct task_struct *threads[NUM_THREADS];
static int thread_ids[NUM_THREADS];

/* 시간 및 카운트 누적 변수 */
static unsigned long long add_to_list_time = 0, add_to_list_count = 0;
static unsigned long long search_time = 0, search_count = 0;
static unsigned long long delete_time = 0, delete_count = 0;

/* 스레드별 범위 설정 */
static void set_iter_range(int thread_id, int range_bound[])
{
    int per_thread = TOTAL_NODES / NUM_THREADS;
    range_bound[0] = (thread_id - 1) * per_thread;
    range_bound[1] = thread_id * per_thread - 1;
}

/* 탐색 타깃 ID 계산 */
static int select_target_index(int range_bound[])
{
    return range_bound[0] + (range_bound[1] - range_bound[0]) / 2;
}

/* Spinlock 기반 삽입 */
static void add_to_list(int thread_id, int range_bound[])
{
    struct timespec64 localclock;
    struct cat *new_cat, *first = NULL;
    int i;

    for (i = range_bound[0]; i <= range_bound[1]; i++) {
        do {
            new_cat = kmalloc(sizeof(struct cat), GFP_KERNEL);
        } while (!new_cat);

        new_cat->var = i;
        INIT_LIST_HEAD(&new_cat->entry);

        if (!first)
            first = new_cat;

        ktime_get_real_ts64(&localclock);
        spin_lock(&head->lock);
        list_add_tail(&new_cat->entry, &head->entry);
        head->total++;
        add_to_list_count++;
        spin_unlock(&head->lock);
        calclock(&localclock, &add_to_list_time, NULL);
    }

    printk(KERN_INFO "thread #%d: inserted cat #%d-%d to the list, total: %u cats\n",
           thread_id, first->var, new_cat->var, head->total);
}

/* Spinlock 기반 탐색 */
static int search_list(int thread_id, int range_bound[])
{
    struct cat *cur;
    struct timespec64 localclock;
    int target_idx = select_target_index(range_bound);

    ktime_get_real_ts64(&localclock);
    spin_lock(&head->lock);
    list_for_each_entry(cur, &head->entry, entry) {
        search_count++;
        if (cur->var == target_idx) {
            spin_unlock(&head->lock);
            calclock(&localclock, &search_time, NULL);
            printk(KERN_INFO "thread #%d: found cat #%d\n", thread_id, target_idx);
            return 0;
        }
    }
    spin_unlock(&head->lock);
    calclock(&localclock, &search_time, NULL);
    return -ENODATA;
}

/* Spinlock 기반 삭제 (target_idx ~ range_bound[1] 범위 삭제) */
static void delete_from_list(int thread_id, int range_bound[])
{
    struct cat *cur, *tmp;
    struct timespec64 localclock;
    int target_idx = select_target_index(range_bound);

    ktime_get_real_ts64(&localclock);
    spin_lock(&head->lock);
    list_for_each_entry_safe(cur, tmp, &head->entry, entry) {
        delete_count++;
        if (cur->var >= target_idx && cur->var <= range_bound[1]) {
            list_del(&cur->entry);
            kfree(cur);
            head->total--;
        }
    }
    spin_unlock(&head->lock);
    calclock(&localclock, &delete_time, NULL);

    printk(KERN_INFO "thread #%d: marked cat #%d-%d as deleted, total: %u cats\n",
           thread_id, target_idx, range_bound[1], head->total);
}

/* 워커 스레드 함수 */
static int work_fn(void *data)
{
    int range_bound[2];
    int err, thread_id = *(int *)data;

    set_iter_range(thread_id, range_bound);
    add_to_list(thread_id, range_bound);

    err = search_list(thread_id, range_bound);
    if (!err)
        delete_from_list(thread_id, range_bound);

    while (!kthread_should_stop()) {
        msleep(500);
    }

    printk(KERN_INFO "thread #%d stopped!\n", thread_id);
    return 0;
}

/* 리스트 해제 */
static void destroy_list(void)
{
    struct cat *cur, *tmp;
    spin_lock(&head->lock);
    list_for_each_entry_safe(cur, tmp, &head->entry, entry) {
        list_del(&cur->entry);
        kfree(cur);
        head->total--;
    }
    spin_unlock(&head->lock);
}

/* 모듈 초기화 */
static int __init spinlock_module_init(void)
{
    int i;
    printk(KERN_INFO "spinlock_module_init: Entering Spinlock Module!\n");

    head = kmalloc(sizeof(struct animal), GFP_KERNEL);
    if (!head)
        return -ENOMEM;

    head->total = 0;
    INIT_LIST_HEAD(&head->entry);
    spin_lock_init(&head->lock);

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

/* 모듈 종료 */
static void __exit spinlock_module_cleanup(void)
{
    int i;

    printk(KERN_INFO "spinlock_module_cleanup: Spinlock linked list insert time: %llu ns, count: %llu\n",
           add_to_list_time, add_to_list_count);
    printk(KERN_INFO "spinlock_module_cleanup: Spinlock linked list search time: %llu ns, count: %llu\n",
           search_time, search_count);
    printk(KERN_INFO "spinlock_module_cleanup: Spinlock linked list delete time: %llu ns, count: %llu\n",
           delete_time, delete_count);

    for (i = 0; i < NUM_THREADS; i++) {
        if (threads[i])
            kthread_stop(threads[i]);
    }

    destroy_list();
    printk(KERN_INFO "After destroyed list: total %d cats\n", head->total);

    kfree(head);
    printk(KERN_INFO "spinlock_module_cleanup: Exiting Spinlock Module!\n");
}

module_init(spinlock_module_init);
module_exit(spinlock_module_cleanup);
