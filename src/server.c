#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include "http.h"

#define WORKER_THREAD_COUNT 4
#define CLIENT_QUEUE_CAPACITY 64

typedef struct
{
    int client_fds[CLIENT_QUEUE_CAPACITY];
    int head;
    int tail;
    int count;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} client_queue_t;

static client_queue_t g_client_queue;
static pthread_t g_worker_threads[WORKER_THREAD_COUNT];

static void client_queue_init(client_queue_t *q)
{
    q->head = 0;
    q->tail = 0;
    q->count = 0;
    if (pthread_mutex_init(&q->mutex, NULL) != 0)
    {
        perror("pthread_mutex_init");
        exit(EXIT_FAILURE);
    }
    if (pthread_cond_init(&q->not_empty, NULL) != 0)
    {
        perror("pthread_cond_init not_empty");
        exit(EXIT_FAILURE);
    }
    if (pthread_cond_init(&q->not_full, NULL) != 0)
    {
        perror("pthread_cond_init not_full");
        exit(EXIT_FAILURE);
    }
}

static void client_queue_push(client_queue_t *q, int client_fd)
{
    pthread_mutex_lock(&q->mutex);
    while (q->count == CLIENT_QUEUE_CAPACITY)
    {
        pthread_cond_wait(&q->not_full, &q->mutex);
    }
    q->client_fds[q->tail] = client_fd;
    q->tail = (q->tail + 1) % CLIENT_QUEUE_CAPACITY;
    q->count++;
    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->mutex);
}

static int client_queue_pop(client_queue_t *q)
{
    pthread_mutex_lock(&q->mutex);
    while (q->count == 0)
    {
        pthread_cond_wait(&q->not_empty, &q->mutex);
    }
    int client_fd = q->client_fds[q->head];
    q->head = (q->head + 1) % CLIENT_QUEUE_CAPACITY;
    q->count--;
    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->mutex);
    return client_fd;
}

static void *worker_thread_main(void *arg)
{
    (void)arg;
    while (1)
    {
        int client_fd = client_queue_pop(&g_client_queue);
        handle_client(client_fd);
        close(client_fd);
    }
    return NULL;
}

void init_thread_pool(void)
{
    client_queue_init(&g_client_queue);
    for (int i = 0; i < WORKER_THREAD_COUNT; ++i)
    {
        if (pthread_create(&g_worker_threads[i], NULL, worker_thread_main, NULL) != 0)
        {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }
    printf("Thread pool initialized with %d workers\n", WORKER_THREAD_COUNT);
}

void enqueue_client(int client_fd)
{
    client_queue_push(&g_client_queue, client_fd);
}

#define LISTENQ 1024

int create_socket_and_listen(int port)
{
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("Error while creating socket");
        exit(EXIT_FAILURE);
    }

    if ((setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))))
    {
        perror("Failure in setsockopt");
        exit(EXIT_FAILURE);
    }

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons((unsigned short)port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("Failure in bind, port may be in use");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, LISTENQ) < 0)
    {
        perror("Failure in listen");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", port);
    return server_fd;
}

int accept_connection(int server_fd)
{
    int new_socket;
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    if ((new_socket = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len)) < 0)
    {
        perror("Failure in accept");
        return -1;
    }

    return new_socket;
}