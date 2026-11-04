#ifndef _CALCLOCK_H_
#define _CALCLOCK_H_

#include <linux/time.h>
#include <linux/ktime.h>

static inline unsigned long long calclock(struct timespec64 *sp, unsigned long long *total_time, unsigned long long *total_count)
{
    unsigned long long timedelay;
    struct timespec64 end;
    ktime_get_real_ts64(&end);

    timedelay = (end.tv_sec - sp->tv_sec) * 1000000000ULL + (end.tv_nsec - sp->tv_nsec);
    if (total_time) *total_time += timedelay;
    if (total_count) (*total_count)++;

    return timedelay;
}

#endif
