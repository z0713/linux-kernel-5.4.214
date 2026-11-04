#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/atomic.h>
#include <linux/errno.h>
#include "calclock.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("CAU SYSLAB");
MODULE_DESCRIPTION("Assignment 7 - Lock-free Linked List Module");

#define NUM_THREADS 4
#define TOTAL_NODES 1000000

/* 슬라이드 25 자료구조 정의 */
struct animal {
    int total;                 /* 리스트 내 전체 노드 수 */
    struct list_head entry;    /* 리스트의 헤드 */
    atomic_t removed;          /* 삭제 여부 플래그 */
    struct list_head gc_entry; /* 가비지 컬렉션 리스트의 헤드 */
};

struct cat {
    int var;                   /* 고양이 ID */
    struct list_head entry;    /* 연결 리스트 엔트리 */
    atomic_t removed;          /* 논리적 삭제 플래그 */
    struct list_head gc_entry; /* 가비지 컬렉션 엔트리 */
};

static struct animal *head;
static struct task_struct *threads[NUM_THREADS];
static int thread_ids[NUM_THREADS];

/* 시간 및 카운트 누적 변수 */
static unsigned long long add_to_list_time = 0, add_to_list_count = 0;
static unsigned long long search_time = 0, search_count = 0;
static unsigned long long delete_time = 0, delete_count = 0;

/* 슬라이드 11, 19: Lock-free list_head 초기화 */
static inline void INIT_LF_LIST_HEAD(struct list_head *list)
{
    list->next = NULL;
    list->prev = list;
}

/* 슬라이드 13, 15: Lock-free 삽입 헬퍼 함수 */
static inline void lf_list_add_tail(struct list_head *entry, struct list_head *head_entry)
{
    entry->prev = __sync_lock_test_and_set(&head_entry->prev, entry);
    if (entry->prev == NULL)
        head_entry->next = entry;
    else
        entry->prev->next = entry;
}

/* 슬라이드 22: 가비지 리스트 삭제 헬퍼 함수 */
static inline void _gc_list_del(struct list_head *prev, struct list_head *next, struct list_head *h)
{
    if (next)
        next->prev = prev;
    else
        h->prev = prev; /* tail 엔트리 갱신 */

    if (prev)
        prev->next = next;
}

static inline void gc_list_del(struct list_head *entry, struct list_head *h)
{
    _gc_list_del(entry->prev, entry->next, h);
}

/* 스레드별 범위 설정 */
static void set_iter_range(int thread_id, int range_bound[])
{
    int per_thread = TOTAL_NODES / NUM_THREADS;
    range_bound[0] = (thread_id - 1) * per_thread;
    range_bound[1] = thread_id * per_thread - 1;
}

/* 탐색 타깃 ID 계산 (중간값) */
static int select_target_index(int range_bound[])
{
    return range_bound[0] + (range_bound[1] - range_bound[0]) / 2;
}

/* 슬라이드 29: Lock-free 삽입 */
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
        INIT_LF_LIST_HEAD(&new_cat->entry);
        INIT_LF_LIST_HEAD(&new_cat->gc_entry);
        atomic_set(&new_cat->removed, 0);

        if (!first)
            first = new_cat;

        ktime_get_real_ts64(&localclock);
        lf_list_add_tail(&new_cat->entry, &head->entry);
        __sync_fetch_and_add(&head->total, 1);
        calclock(&localclock, &add_to_list_time, &add_to_list_count);
    }

    printk(KERN_INFO "thread #%d: inserted cat #%d-%d to the list, total: %u cats\n",
           thread_id, first->var, new_cat->var, head->total);
}

/* 슬라이드 30, 17: Lock-free 탐색 */
static int search_list(int thread_id, int range_bound[])
{
    struct list_head *entry, *iter = head->entry.next;
    struct cat *cur;
    struct timespec64 localclock;
    int target_idx = select_target_index(range_bound);

    ktime_get_real_ts64(&localclock);
    while ((entry = iter) != NULL) {
        search_count++;
        if (__sync_val_compare_and_swap(&iter, entry, iter->next) != entry)
            continue;

        if (!entry->prev || !entry->prev->next)
            continue;

        cur = list_entry(entry, struct cat, entry);
        if (atomic_read(&cur->removed))
            continue;

        if (cur->var == target_idx) {
            calclock(&localclock, &search_time, NULL);
            printk(KERN_INFO "thread #%d: found cat #%d\n", thread_id, target_idx);
            return 0;
        }
    }
    calclock(&localclock, &search_time, NULL);
    return -ENODATA;
}

/* 슬라이드 34 요구 템플릿에 맞춘 논리적 삭제 */
static void delete_from_list(int thread_id, int range_bound[])
{
    struct list_head *entry, *iter = head->entry.next;
    struct cat *cur;
    struct timespec64 localclock;
    int target_idx = select_target_index(range_bound);

    ktime_get_real_ts64(&localclock);
    while ((entry = iter) != NULL) {
        delete_count++;
        if (__sync_val_compare_and_swap(&iter, entry, iter->next) != entry)
            continue;

        if (!entry->prev || !entry->prev->next)
            continue;

        cur = list_entry(entry, struct cat, entry);
        if (atomic_read(&cur->removed))
            continue;

        /* 슬라이드 34 기준: target_idx부터 range_bound[1]까지 논리적 삭제 수행 */
        if (cur->var >= target_idx && cur->var <= range_bound[1]) {
            atomic_set(&cur->removed, 1);
            lf_list_add_tail(&cur->gc_entry, &head->gc_entry);
            __sync_fetch_and_sub(&head->total, 1);
        }
    }
    calclock(&localclock, &delete_time, NULL);

    printk(KERN_INFO "thread #%d: marked cat #%d-%d as deleted, total: %u cats\n",
           thread_id, target_idx, range_bound[1], head->total);
}

/* 슬라이드 28: 워커 스레드 함수 */
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

/* 슬라이드 32, 21: 가비지 리스트 비우기 (물리적 메모리 해제) */
static int empty_garbage_list(void)
{
    struct cat *cur;
    struct list_head *entry, *iter = head->gc_entry.next;
    unsigned int counter = 0;

    while ((entry = iter) != NULL) {
        iter = iter->next;
        cur = list_entry(entry, struct cat, gc_entry);

        gc_list_del(&cur->entry, &head->entry);
        gc_list_del(&cur->gc_entry, &head->gc_entry);
        kfree(cur);
        counter++;
    }

    printk(KERN_INFO "empty_garbage_list: freed %u cats\n", counter);
    return 0;
}

/* 슬라이드 33: 남은 리스트 전체 삭제 */
static void destroy_list(void)
{
    struct cat *cur;
    struct list_head *entry, *iter = head->entry.next;

    while ((entry = iter) != NULL) {
        iter = iter->next;
        cur = list_entry(entry, struct cat, entry);
        gc_list_del(&cur->entry, &head->entry);
        kfree(cur);
        __sync_fetch_and_sub(&head->total, 1);
    }
}

/* 슬라이드 27: 모듈 초기화 */
static int __init lockfree_module_init(void)
{
    int i;
    printk(KERN_INFO "lockfree_module_init: Entering Lock-free Module!\n");

    head = kmalloc(sizeof(struct animal), GFP_KERNEL);
    if (!head)
        return -ENOMEM;

    head->total = 0;
    INIT_LF_LIST_HEAD(&head->entry);
    INIT_LF_LIST_HEAD(&head->gc_entry);
    atomic_set(&head->removed, 0);

    for (i = 0; i < NUM_THREADS; i++) {
        thread_ids[i] = i + 1;
        threads[i] = kthread_run(work_fn, &thread_ids[i], "lf_thread_%d", i + 1);
        if (IS_ERR(threads[i])) {
            printk(KERN_ERR "Failed to create thread %d\n", i + 1);
            return PTR_ERR(threads[i]);
        }
    }

    return 0;
}

/* 슬라이드 31, 34: 모듈 종료 */
static void __exit lockfree_module_cleanup(void)
{
    int i;

    /* 슬라이드 34 형식에 맞춘 시간 측정 통계 출력 */
    printk(KERN_INFO "lockfree_module_cleanup: Lock-free linked list insert time: %llu ns, count: %llu\n",
           add_to_list_time, add_to_list_count);
    printk(KERN_INFO "lockfree_module_cleanup: Lock-free linked list search time: %llu ns, count: %llu\n",
           search_time, search_count);
    printk(KERN_INFO "lockfree_module_cleanup: Lock-free linked list delete time: %llu ns, count: %llu\n",
           delete_time, delete_count);

    for (i = 0; i < NUM_THREADS; i++) {
        if (threads[i])
            kthread_stop(threads[i]);
    }

    empty_garbage_list();
    printk(KERN_INFO "After gc: total %d cats\n", head->total);

    destroy_list();
    printk(KERN_INFO "After destroyed list: total %d cats\n", head->total);

    kfree(head);
    printk(KERN_INFO "lockfree_module_cleanup: Exiting Lock-free Module!\n");
}

module_init(lockfree_module_init);
module_exit(lockfree_module_cleanup);
