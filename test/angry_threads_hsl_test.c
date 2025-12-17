// hls_stress_test.c
// Simula clientes HLS: baja el playlist .m3u8 y luego baja N segmentos listados.
// Compilar:
//   gcc -O2 -pthread -o hls_stress_test hls_stress_test.c
// Uso:
//   ./hls_stress_test <host> <port> <threads> <iterations_per_thread> <playlist_path> <segments_to_fetch>

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
    int iterations_per_thread;
    const char *playlist_path;
    int segments_to_fetch;
} worker_args_t;

static int connect_to_host(const char *host, int port)
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        return -1;

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons((unsigned short)port);

    if (inet_pton(AF_INET, host, &serv_addr.sin_addr) <= 0)
    {
        close(sockfd);
        errno = EINVAL;
        return -1;
    }

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        close(sockfd);
        return -1;
    }

    return sockfd;
}

static int send_all(int fd, const void *buf, size_t len)
{
    const char *p = (const char *)buf;
    while (len > 0)
    {
        ssize_t n = send(fd, p, len, 0);
        if (n <= 0)
            return -1;
        p += (size_t)n;
        len -= (size_t)n;
    }
    return 0;
}

// Lee todo hasta que el server cierre (Connection: close). Devuelve buffer heap (caller free) y tamaño.
// Si no querés guardar body, podés pasar out_buf=NULL para solo drenar.
static int http_get(const char *host, int port, const char *path, char **out_body, size_t *out_body_len)
{
    int sockfd = connect_to_host(host, port);
    if (sockfd < 0)
        return -1;

    char req[1024];
    int req_len = snprintf(req, sizeof(req),
                           "GET %s HTTP/1.1\r\n"
                           "Host: %s\r\n"
                           "Connection: close\r\n"
                           "\r\n",
                           path, host);

    if (req_len <= 0 || req_len >= (int)sizeof(req))
    {
        close(sockfd);
        errno = EOVERFLOW;
        return -1;
    }

    if (send_all(sockfd, req, (size_t)req_len) < 0)
    {
        close(sockfd);
        return -1;
    }

    // Leer todo
    size_t cap = 0;
    size_t len = 0;
    char *buf = NULL;

    char tmp[4096];
    while (1)
    {
        ssize_t n = recv(sockfd, tmp, sizeof(tmp), 0);
        if (n < 0)
        {
            free(buf);
            close(sockfd);
            return -1;
        }
        if (n == 0)
            break;

        if (out_body)
        {
            if (len + (size_t)n + 1 > cap)
            {
                size_t newcap = (cap == 0) ? 8192 : cap * 2;
                while (newcap < len + (size_t)n + 1)
                    newcap *= 2;
                char *nb = (char *)realloc(buf, newcap);
                if (!nb)
                {
                    free(buf);
                    close(sockfd);
                    errno = ENOMEM;
                    return -1;
                }
                buf = nb;
                cap = newcap;
            }
            memcpy(buf + len, tmp, (size_t)n);
            len += (size_t)n;
        }
    }

    close(sockfd);

    if (out_body)
    {
        if (!buf)
        {
            buf = (char *)malloc(1);
            if (!buf)
                return -1;
            buf[0] = '\0';
            len = 0;
        }
        else
        {
            buf[len] = '\0';
        }

        // Separar headers/body: buscar \r\n\r\n
        char *sep = strstr(buf, "\r\n\r\n");
        if (!sep)
        {
            // respuesta rara, devuelvo todo como body
            *out_body = buf;
            *out_body_len = len;
            return 0;
        }

        char *body = sep + 4;
        size_t body_len = len - (size_t)(body - buf);

        // Copiar body a un buffer exacto
        char *body_copy = (char *)malloc(body_len + 1);
        if (!body_copy)
        {
            free(buf);
            errno = ENOMEM;
            return -1;
        }
        memcpy(body_copy, body, body_len);
        body_copy[body_len] = '\0';

        free(buf);

        *out_body = body_copy;
        *out_body_len = body_len;
    }

    return 0;
}

// Extrae hasta max_paths segmentos del playlist (líneas que no empiezan con '#').
// Devuelve count, y rellena seg_paths con strings heap (caller free).
static int parse_m3u8_segments(const char *playlist_body, char **seg_paths, int max_paths)
{
    int count = 0;

    const char *p = playlist_body;
    while (*p && count < max_paths)
    {
        // tomar línea
        const char *line_start = p;
        const char *line_end = strchr(p, '\n');
        if (!line_end)
            line_end = p + strlen(p);

        size_t line_len = (size_t)(line_end - line_start);
        while (line_len > 0 && (line_start[line_len - 1] == '\r' || line_start[line_len - 1] == '\n'))
            line_len--;

        // avanzar p
        p = (*line_end == '\0') ? line_end : line_end + 1;

        if (line_len == 0)
            continue;
        if (line_start[0] == '#')
            continue;

        // copiar path
        char *s = (char *)malloc(line_len + 1);
        if (!s)
            break;
        memcpy(s, line_start, line_len);
        s[line_len] = '\0';

        seg_paths[count++] = s;
    }

    return count;
}

// Une base_dir (ej: "/hls/movie1/") + segment ("seg001.ts") o si segment ya es absoluto "/hls/..."
// devuelve string heap.
static char *join_path(const char *base_dir, const char *segment)
{
    if (segment[0] == '/')
    {
        return strdup(segment);
    }
    size_t a = strlen(base_dir);
    size_t b = strlen(segment);

    int need_slash = (a > 0 && base_dir[a - 1] != '/');

    char *out = (char *)malloc(a + (need_slash ? 1 : 0) + b + 1);
    if (!out)
        return NULL;

    memcpy(out, base_dir, a);
    size_t pos = a;
    if (need_slash)
        out[pos++] = '/';
    memcpy(out + pos, segment, b);
    out[pos + b] = '\0';
    return out;
}

// Dado "/hls/movie1/index.m3u8" -> "/hls/movie1/"
static void get_base_dir(const char *playlist_path, char *out, size_t outsz)
{
    const char *last = strrchr(playlist_path, '/');
    if (!last)
    {
        snprintf(out, outsz, "/");
        return;
    }
    size_t n = (size_t)(last - playlist_path) + 1; // incluye '/'
    if (n >= outsz)
        n = outsz - 1;
    memcpy(out, playlist_path, n);
    out[n] = '\0';
}

static void *worker_main(void *arg)
{
    worker_args_t *w = (worker_args_t *)arg;

    char base_dir[512];
    get_base_dir(w->playlist_path, base_dir, sizeof(base_dir));

    for (int it = 0; it < w->iterations_per_thread; ++it)
    {
        // 1) Bajar playlist
        char *playlist_body = NULL;
        size_t playlist_len = 0;

        if (http_get(w->host, w->port, w->playlist_path, &playlist_body, &playlist_len) < 0)
        {
            perror("GET playlist");
            continue;
        }

        // 2) Parsear segmentos
        int max_segs = w->segments_to_fetch;
        if (max_segs <= 0)
            max_segs = 1;
        char **seg_names = (char **)calloc((size_t)max_segs, sizeof(char *));
        if (!seg_names)
        {
            free(playlist_body);
            perror("calloc seg_names");
            continue;
        }

        int seg_count = parse_m3u8_segments(playlist_body, seg_names, max_segs);
        free(playlist_body);

        // 3) Descargar segmentos
        for (int s = 0; s < seg_count; ++s)
        {
            char *seg_path = join_path(base_dir, seg_names[s]);
            if (!seg_path)
            {
                perror("join_path");
                continue;
            }

            // body no lo guardamos: solo drenamos respuesta
            if (http_get(w->host, w->port, seg_path, NULL, NULL) < 0)
            {
                perror("GET segment");
            }

            free(seg_path);
        }

        for (int s = 0; s < seg_count; ++s)
            free(seg_names[s]);
        free(seg_names);
    }

    return NULL;
}

int main(int argc, char *argv[])
{
    if (argc < 7)
    {
        fprintf(stderr,
                "Uso: %s <host> <port> <threads> <iterations_per_thread> <playlist_path> <segments_to_fetch>\n"
                "Ejemplo: %s 127.0.0.1 8000 8 50 /hls/movie1/index.m3u8 20\n",
                argv[0], argv[0]);
        return EXIT_FAILURE;
    }

    const char *host = argv[1];
    int port = atoi(argv[2]);
    int thread_count = atoi(argv[3]);
    int iterations_per_thread = atoi(argv[4]);
    const char *playlist_path = argv[5];
    int segments_to_fetch = atoi(argv[6]);

    if (thread_count <= 0 || iterations_per_thread <= 0 || segments_to_fetch <= 0)
    {
        fprintf(stderr, "threads, iterations_per_thread y segments_to_fetch deben ser > 0\n");
        return EXIT_FAILURE;
    }

    printf("HLS stress test contra %s:%d\n", host, port);
    printf("Hilos: %d | iteraciones/hilo: %d | playlist: %s | segmentos/sesion: %d\n",
           thread_count, iterations_per_thread, playlist_path, segments_to_fetch);

    pthread_t *threads = (pthread_t *)malloc(sizeof(pthread_t) * (size_t)thread_count);
    if (!threads)
    {
        perror("malloc threads");
        return EXIT_FAILURE;
    }

    worker_args_t wargs = {
        .host = host,
        .port = port,
        .iterations_per_thread = iterations_per_thread,
        .playlist_path = playlist_path,
        .segments_to_fetch = segments_to_fetch};

    for (int i = 0; i < thread_count; ++i)
    {
        if (pthread_create(&threads[i], NULL, worker_main, &wargs) != 0)
        {
            perror("pthread_create");
            free(threads);
            return EXIT_FAILURE;
        }
    }

    for (int i = 0; i < thread_count; ++i)
    {
        pthread_join(threads[i], NULL);
    }

    free(threads);

    printf("HLS stress test terminado.\n");
    return EXIT_SUCCESS;
}
