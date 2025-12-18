#include "server.h"
#include "http.h"
#include "logger.h"
#include "stats.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <getopt.h>

static volatile int keep_running = 1;
static char *g_log_file = NULL;

static struct termios orig_termios;

void set_nonblocking_input(void)
{
    tcgetattr(STDIN_FILENO, &orig_termios);

    struct termios new_termios = orig_termios;
    new_termios.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);

    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
}

void restore_blocking_input(void)
{
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);

    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags & ~O_NONBLOCK);
}

int check_quit_key(void)
{
    char c;
    if (read(STDIN_FILENO, &c, 1) == 1)
    {
        if (c == 'q' || c == 'Q')
        {
            return 1;
        }
    }
    return 0;
}

void cleanup_and_exit(void)
{
    printf("\n\nShutting down server...\n");
    restore_blocking_input();
    print_stats(g_log_file);
    exit(0);
}

int main(int argc, char *argv[])
{

    int func_opt;
    int worker_count = 4;
    int enable_logging = 0;

    static struct option long_options[] = {
        {"workers", required_argument, 0, 'n'},
        {"output", required_argument, 0, 'o'},
        {"log", no_argument, 0, 'l'},
        {0, 0, 0, 0}};

    while ((func_opt = getopt_long(argc, argv, "n:o:l", long_options, NULL)) != -1)
    {
        switch (func_opt)
        {
        case 'n':
            worker_count = atoi(optarg);
            if (worker_count < 1)
                worker_count = 1;
            if (worker_count > 100)
                worker_count = 100;
            break;
        case 'o':
            g_log_file = strdup(optarg);
            break;
        case 'l':
            enable_logging = 1;
            break;
        default:
            fprintf(stderr, "Usage: %s [-n workers] [-o output_file] [-l|--log]\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    if (enable_logging)
    {
        init_logger(worker_count, g_log_file);
    }

    init_stats();
    init_thread_pool(worker_count);
    int server_fd = create_socket_and_listen(8001);

    if (!is_logging_enabled())
    {
        printf("Servidor HTTP escuchando en puerto 8000...\n");
        printf("Workers: %d\n", worker_count);
        if (g_log_file)
            printf("Logging to file: %s\n", g_log_file);
        printf("Presiona 'q' para salir y ver estadísticas\n");
    }

    set_nonblocking_input();

    while (keep_running)
    {
        if (check_quit_key())
        {
            cleanup_and_exit();
        }

        log_event(ID_PRODUCER, -1, "Sleeping", "Waiting for connection");

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100000;
        setsockopt(server_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        int client_fd = accept_connection(server_fd);
        if (client_fd < 0)
            continue;

        log_event(ID_PRODUCER, client_fd, "Running", "New connection accepted");
        enqueue_client(client_fd);
        log_event(ID_PRODUCER, client_fd, "Ready", "Client added to queue");
    }

    cleanup_and_exit();
    return 0;
}