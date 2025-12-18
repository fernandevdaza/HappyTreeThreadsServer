#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

static int g_worker_count = 0;
static char (*current_states)[32] = NULL;
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
static FILE *log_output = NULL;
static int logging_enabled = 0;

void init_logger(int worker_count, const char *logfile)
{
    pthread_mutex_lock(&log_mutex);

    g_worker_count = worker_count;

    int total_slots = 1 + worker_count;
    current_states = malloc(total_slots * sizeof(*current_states));
    if (!current_states)
    {
        perror("Failed to allocate logger memory");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < total_slots; i++)
    {
        strcpy(current_states[i], "Ready");
    }

    if (logfile)
    {
        log_output = fopen(logfile, "w");
        if (!log_output)
        {
            perror("Failed to open log file");
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        log_output = stdout;
    }

    logging_enabled = 1;

    fprintf(log_output, "%-12s | %-6s | %-10s", "Timestamp", "FD", "Producer");
    for (int i = 0; i < worker_count; i++)
    {
        char label[16];
        snprintf(label, sizeof(label), "Worker %d", i);
        fprintf(log_output, " | %-10s", label);
    }
    fprintf(log_output, " | %s\n", "Comments");

    fprintf(log_output, "--------------------------------------------------");
    for (int i = 0; i < worker_count; i++)
    {
        fprintf(log_output, "-------------");
    }
    fprintf(log_output, "\n");

    fflush(log_output);
    pthread_mutex_unlock(&log_mutex);
}

int is_logging_enabled(void)
{
    return logging_enabled;
}

void log_event(int entity_id, int client_fd, const char *state, const char *comment)
{
    if (!logging_enabled || !log_output)
    {
        return;
    }
    pthread_mutex_lock(&log_mutex);

    int slot_index;
    if (entity_id == ID_PRODUCER)
    {
        slot_index = 0;
    }
    else if (entity_id >= 0 && entity_id < g_worker_count)
    {
        slot_index = entity_id + 1;
    }
    else
    {
        pthread_mutex_unlock(&log_mutex);
        return;
    }

    if (state != NULL)
    {
        snprintf(current_states[slot_index], sizeof(current_states[slot_index]), "%s", state);
    }

    char time_buf[64];
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm *tm_info = localtime(&tv.tv_sec);
    strftime(time_buf, sizeof(time_buf), "%H:%M:%S", tm_info);

    char timestamp[128];
    snprintf(timestamp, sizeof(timestamp), "%s.%03ld", time_buf, tv.tv_usec / 1000);

    char fd_str[16];
    if (client_fd >= 0)
    {
        snprintf(fd_str, sizeof(fd_str), "%d", client_fd);
    }
    else
    {
        strcpy(fd_str, "-");
    }

    fprintf(log_output, "%-12s | %-6s | %-10s", timestamp, fd_str, current_states[0]);

    for (int i = 0; i < g_worker_count; i++)
    {
        fprintf(log_output, " | %-10s", current_states[i + 1]);
    }

    fprintf(log_output, " | %s\n", comment ? comment : "");

    fflush(log_output);
    pthread_mutex_unlock(&log_mutex);
}
