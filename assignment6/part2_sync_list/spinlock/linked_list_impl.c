#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include "calclock.h"

struct my_node {
    int data;
    struct list_head list;
};

static LIST_HEAD(my_list);
static DEFINE_SPINLOCK(my_lock);

unsigned long long insert_time = 0, insert_count = 0;
unsigned long long search_time = 0, search_count = 0;
unsigned long long delete_time = 0, delete_count = 0;

void *add_to_list(int thread_id, int range_bound[])
{
    int i;
    struct my_node *first = NULL;
    struct timespec64 localclock;

    printk(KERN_INFO "thread #%d range: %d ~ %d\n", thread_id, range_bound[0], range_bound[1]);

    for (i = range_bound[0]; i <= range_bound[1]; i++) {
        struct my_node *new_node = kmalloc(sizeof(struct my_node), GFP_KERNEL);
        if (!new_node)
            continue;
        new_node->data = i;

        ktime_get_real_ts64(&localclock);
        spin_lock(&my_lock);
        list_add_tail(&new_node->list, &my_list);
        insert_count++;
	spin_unlock(&my_lock);
        calclock(&localclock, &insert_time, NULL);

        if (!first)
            first = new_node;
    }

    return (void *)first;
}

int search_list(int thread_id, void *data, int range_bound[])
{
    struct my_node *cur;
    struct timespec64 localclock;

    ktime_get_real_ts64(&localclock);
    spin_lock(&my_lock);
    list_for_each_entry(cur, &my_list, list) {
        if (cur->data >= range_bound[0] && cur->data <= range_bound[1]) {
            search_count++;
        }
    }
    spin_unlock(&my_lock);
    calclock(&localclock, &search_time, NULL);

    printk(KERN_INFO "thread #%d searched range: %d ~ %d\n", thread_id, range_bound[0], range_bound[1]);
    return 0;
}

int delete_from_list(int thread_id, int range_bound[])
{
    struct my_node *cur, *tmp;
    struct timespec64 localclock;

    ktime_get_real_ts64(&localclock);
    spin_lock(&my_lock);
    list_for_each_entry_safe(cur, tmp, &my_list, list) {
        if (cur->data >= range_bound[0] && cur->data <= range_bound[1]) {
            list_del(&cur->list);
            kfree(cur);
	    delete_count++;
        }
    }
    spin_unlock(&my_lock);
    calclock(&localclock, &delete_time, NULL);

    printk(KERN_INFO "thread #%d deleted range: %d ~ %d\n", thread_id, range_bound[0], range_bound[1]);
    return 0;
}
