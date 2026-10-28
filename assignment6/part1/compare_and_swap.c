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
static int counter = 0; // 4개 스레드가 공유하는 카운터

static int compare_and_swap_function(void *data)
{
    while (!kthread_should_stop()) {
        int old_val;
        int new_val;

        // CAS 연산: counter 값이 old_val과 같을 때만 new_val로 갱신
        do {
            old_val = counter;
            new_val = old_val + 1;
        } while (cmpxchg(&counter, old_val, new_val) != old_val);

        // 슬라이드 32 포맷에 맞춘 커널 로그 출력
        printk(KERN_INFO "pid [%d] compare_and_swap_function: counter: %d\n", current->pid, new_val);

        // 출력 속도 조절을 위한 0.5초 대기
        msleep(500);
    }
    return 0;
}

static int __init compare_and_swap_module_init(void)
{
    int i;
    // 진입 로그 출력
    printk(KERN_INFO "compare_and_swap_module_init: Entering Compare and swap Module!\n");

    // 4개의 커널 스레드 생성 및 실행
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

    // 모듈 해제 시 실행 중인 4개의 스레드 종료
    for (i = 0; i < NUM_THREADS; i++) {
        if (threads[i]) {
            kthread_stop(threads[i]);
        }
    }

    // 종료 로그 출력
    printk(KERN_INFO "compare_and_swap_module_cleanup: Exiting Compare and Swap Module!\n");
}

module_init(compare_and_swap_module_init);
module_exit(compare_and_swap_module_cleanup);
