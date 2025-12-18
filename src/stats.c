#include "stats.h"
#include <stdio.h>
#include <time.h>

static server_stats_t g_stats = {0};

void init_stats(void) {
    atomic_init(&g_stats.total_requests, 0);
    atomic_init(&g_stats.successful_requests, 0);
    atomic_init(&g_stats.failed_requests, 0);
    atomic_init(&g_stats.bytes_sent, 0);
    atomic_init(&g_stats.total_turnaround_time_us, 0);
    atomic_init(&g_stats.total_response_time_us, 0);
    g_stats.start_time = time(NULL);
}

void increment_requests(void) {
    atomic_fetch_add(&g_stats.total_requests, 1);
}

void increment_successful(void) {
    atomic_fetch_add(&g_stats.successful_requests, 1);
}

void increment_failed(void) {
    atomic_fetch_add(&g_stats.failed_requests, 1);
}

void add_bytes_sent(unsigned long bytes) {
    atomic_fetch_add(&g_stats.bytes_sent, bytes);
}

void add_turnaround_time(unsigned long long microseconds) {
    atomic_fetch_add(&g_stats.total_turnaround_time_us, microseconds);
}

void add_response_time(unsigned long long microseconds) {
    atomic_fetch_add(&g_stats.total_response_time_us, microseconds);
}

void print_stats(void) {
    time_t end_time = time(NULL);
    double uptime = difftime(end_time, g_stats.start_time);
    
    unsigned long total = atomic_load(&g_stats.total_requests);
    unsigned long successful = atomic_load(&g_stats.successful_requests);
    unsigned long failed = atomic_load(&g_stats.failed_requests);
    unsigned long long bytes = atomic_load(&g_stats.bytes_sent);
    unsigned long long total_turnaround_us = atomic_load(&g_stats.total_turnaround_time_us);
    unsigned long long total_response_us = atomic_load(&g_stats.total_response_time_us);
    
    double avg_turnaround_ms = (total > 0) ? (total_turnaround_us / (double)total) / 1000.0 : 0.0;
    double avg_response_ms = (total > 0) ? (total_response_us / (double)total) / 1000.0 : 0.0;
    
    printf("\n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("                   SERVER STATISTICS                       \n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("  Uptime:              %.0f seconds\n", uptime);
    printf("  Total Requests:      %lu\n", total);
    printf("  Successful:          %lu\n", successful);
    printf("  Failed:              %lu\n", failed);
    printf("  Bytes Sent:          %llu (%.2f MB)\n", bytes, bytes / 1024.0 / 1024.0);
    if (uptime > 0) {
        printf("  Requests/sec:        %.2f\n", total / uptime);
        printf("  Throughput:          %.2f KB/s\n", (bytes / 1024.0) / uptime);
    }
    printf("  Avg Turnaround Time: %.2f ms\n", avg_turnaround_ms);
    printf("  Avg Response Time:   %.2f ms\n", avg_response_ms);
    printf("═══════════════════════════════════════════════════════════\n");
    printf("\n");
}
