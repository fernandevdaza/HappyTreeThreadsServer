#ifndef STATS_H
#define STATS_H

#include <stdatomic.h>
#include <time.h>

typedef struct
{
    atomic_ulong total_requests;
    atomic_ulong successful_requests;
    atomic_ulong failed_requests;
    atomic_ulong aborted_requests;
    atomic_ullong bytes_sent;
    atomic_ullong total_turnaround_time_us;
    atomic_ullong total_response_time_us;
    time_t start_time;
} server_stats_t;

void init_stats(void);
void increment_requests(void);
void increment_successful(void);
void increment_failed(void);
void increment_aborted(void);
void add_bytes_sent(unsigned long bytes);
void add_turnaround_time(unsigned long long microseconds);
void add_response_time(unsigned long long microseconds);
void print_stats(const char *outfile);

#endif
