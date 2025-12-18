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

static volatile int keep_running = 1;

// Set stdin to non-blocking mode
void set_nonblocking_input(void) {
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
}

// Restore stdin to blocking mode
void restore_blocking_input(void) {
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags & ~O_NONBLOCK);
}

// Check if 'q' was pressed
int check_quit_key(void) {
    char c;
    if (read(STDIN_FILENO, &c, 1) == 1) {
        if (c == 'q' || c == 'Q') {
            return 1;
        }
    }
    return 0;
}

void cleanup_and_exit(void) {
    printf("\n\nShutting down server...\n");
    restore_blocking_input();
    print_stats();
    exit(0);
}

int main(int argc, char *argv[])
{
//    signal(SIGPIPE, SIG_IGN);
    
    int enable_logging = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--log") == 0 || strcmp(argv[i], "-l") == 0) {
            enable_logging = 1;
            break;
        }
    }
    
    if (enable_logging) {
        init_logger();
    }
    
    init_stats();
    init_thread_pool();
    int server_fd = create_socket_and_listen(8000);

    if (!is_logging_enabled()) {
        printf("Servidor HTTP escuchando en puerto 8000...\n");
        printf("Presiona 'q' para salir y ver estadísticas\n");
    }

    set_nonblocking_input();

    while (keep_running)   
    {
        // Check for quit key
        if (check_quit_key()) {
            cleanup_and_exit();
        }

        log_event(ID_PRODUCER, -1, "Sleep", "Waiting for connection");
        
        // Set accept timeout to allow periodic quit checks
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100000; // 100ms
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