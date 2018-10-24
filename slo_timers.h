/*
 * Copyright (C) 2017 Santiago León O. <santileortiz@gmail.com>
 */

#if !defined(SLO_TIMERS_H)
#include <time.h>

// Functions used for profiling and timming in general:
//  - A process clock that measures time used by this process.
//  - A wall clock that measures real world time.

struct timespec proc_clock_info;
struct timespec wall_clock_info;
void  validate_clock (struct timespec *clock_info) {
    if (clock_info->tv_sec != 0 || clock_info->tv_nsec != 1) {
        printf ("Error: We expect a 1ns resolution timer. %li s %li ns resolution instead.\n",
                clock_info->tv_sec, clock_info->tv_nsec);
    }
}

void setup_clocks ()
{
    if (-1 == clock_getres (CLOCK_PROCESS_CPUTIME_ID, &proc_clock_info)) {
        printf ("Error: could not get profiling clock resolution\n");
    }
    validate_clock (&proc_clock_info);

    if (-1 == clock_getres (CLOCK_MONOTONIC, &wall_clock_info)) {
        printf ("Error: could not get wall clock resolution\n");
    }
    validate_clock (&wall_clock_info);
    return;
}

void print_time_elapsed (struct timespec *time_start, struct timespec *time_end, char *str)
{
    float time_in_ms = (time_end->tv_sec*1000-time_start->tv_sec*1000+
                       (float)(time_end->tv_nsec-time_start->tv_nsec)/1000000);
    if (time_in_ms < 0.0001) {
        printf ("[%ld ns]: %s\n", time_end->tv_nsec-time_start->tv_nsec, str);
    } else if (time_in_ms < 1) {
        printf ("[%f us]: %s\n", time_in_ms*1000, str);
    } else if (time_in_ms < 1000) {
        printf ("[%f ms]: %s\n", time_in_ms, str);
    } else {
        printf ("[%f s]: %s\n", time_in_ms/1000, str);
    }
}

float time_elapsed_in_ms (struct timespec *time_start, struct timespec *time_end)
{
    return (time_end->tv_sec*1000-time_start->tv_sec*1000+
           (float)(time_end->tv_nsec-time_start->tv_nsec)/1000000);
}

// Process clock
// Usage:
//   BEGIN_TIME_BLOCK;
//   <some code to measure>
//   END_TIME_BLOCK("Name of timed block");

struct timespec proc_ticks_start;
struct timespec proc_ticks_end;
#define BEGIN_TIME_BLOCK {clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &proc_ticks_start);}
#define END_TIME_BLOCK(str) {\
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &proc_ticks_end);\
    print_time_elapsed(&proc_ticks_start, &proc_ticks_end, str);\
    }

// Wall timer
// Usage:
//   BEGIN_WALL_CLOCK; //call once
//   <arbitrary code>
//   PROBE_WALL_CLOCK("Name of probe 1");
//   <arbitrary code>
//   PROBE_WALL_CLOCK("Name of probe 2");

struct timespec wall_ticks_start;
struct timespec wall_ticks_end;
#define BEGIN_WALL_CLOCK {clock_gettime(CLOCK_MONOTONIC, &wall_ticks_start);}
#define PROBE_WALL_CLOCK(str) {\
    clock_gettime(CLOCK_MONOTONIC, &wall_ticks_end);\
    print_time_elapsed(&wall_ticks_start, &wall_ticks_end, str);\
    wall_ticks_start = wall_ticks_end;\
    }

#define SLO_TIMERS_H
#endif
