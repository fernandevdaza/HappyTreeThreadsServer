// stress_test.c
// Pequeño load tester para el servidor HTTP en C.
// Uso:
//   ./stress_test 127.0.0.1 8000 4 1000
//     host      = 127.0.0.1 (o localhost)
//     port      = 8000
//     threads   = 4   (número de hilos cliente)
//     requests  = 1000 (requests por hilo)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

typedef struct
{
    const char *host;
    int port;
    int requests_per_thread;
    const char *path;
} worker_args_t;

static void *worker_main(void *arg)
{
    worker_args_t *w = (worker_args_t *)arg;

    char request[512];
    snprintf(request, sizeof(request),
             "GET %s HTTP/1.0\r\n"
             "Host: %s\r\n"
             "Connection: close\r\n"
             "\r\n",
             w->path, w->host);

    for (int i = 0; i < w->requests_per_thread; ++i)
    {
        int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0)
        {
            perror("socket");
            continue;
        }

        struct sockaddr_in serv_addr;
        memset(&serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons((unsigned short)w->port);

        if (inet_pton(AF_INET, w->host, &serv_addr.sin_addr) <= 0)
        {
            perror("inet_pton");
            close(sockfd);
            continue;
        }

        if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        {
            perror("connect");
            close(sockfd);
            continue;
        }

        ssize_t sent = send(sockfd, request, strlen(request), 0);
        if (sent < 0)
        {
            perror("send");
            close(sockfd);
            continue;
        }

        // Leer la respuesta y descartarla
        char buffer[4096];
        while (1)
        {
            ssize_t n = recv(sockfd, buffer, sizeof(buffer), 0);
            if (n < 0)
            {
                perror("recv");
                break;
            }
            if (n == 0)
            {
                break; // el servidor cerró la conexión
            }
            // No hacemos nada con los datos, solo los consumimos
        }

        close(sockfd);
    }

    return NULL;
}

int main(int argc, char *argv[])
{
    if (argc < 5)
    {
        fprintf(stderr,
                "Uso: %s <host> <port> <threads> <requests_per_thread> [path]\n"
                "Ejemplo: %s 127.0.0.1 8000 4 1000 /\n",
                argv[0], argv[0]);
        return EXIT_FAILURE;
    }

    const char *host = argv[1];
    int port = atoi(argv[2]);
    int thread_count = atoi(argv[3]);
    int requests_per_thread = atoi(argv[4]);
    const char *path = (argc >= 6) ? argv[5] : "/";

    if (thread_count <= 0 || requests_per_thread <= 0)
    {
        fprintf(stderr, "threads y requests_per_thread deben ser > 0\n");
        return EXIT_FAILURE;
    }

    printf("Stress test contra %s:%d\n", host, port);
    printf("Hilos: %d, requests por hilo: %d, path: %s\n",
           thread_count, requests_per_thread, path);

    pthread_t *threads = malloc(sizeof(pthread_t) * (size_t)thread_count);
    if (!threads)
    {
        perror("malloc threads");
        return EXIT_FAILURE;
    }

    worker_args_t wargs = {
        .host = host,
        .port = port,
        .requests_per_thread = requests_per_thread,
        .path = path};

    for (int i = 0; i < thread_count; ++i)
    {
        if (pthread_create(&threads[i], NULL, worker_main, &wargs) != 0)
        {
            perror("pthread_create");
            free(threads);
            return EXIT_FAILURE;
        }
    }

    // Esperar que terminen todos los hilos
    for (int i = 0; i < thread_count; ++i)
    {
        pthread_join(threads[i], NULL);
    }

    free(threads);

    printf("Stress test terminado.\n");
    return EXIT_SUCCESS;
}
