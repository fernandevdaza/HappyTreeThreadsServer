#ifndef LOGGER_H
#define LOGGER_H

#define ID_PRODUCER -1

void init_logger(int worker_count, const char *logfile);
int is_logging_enabled(void);
void log_event(int entity_id, int client_fd, const char *state, const char *comment);

#endif
