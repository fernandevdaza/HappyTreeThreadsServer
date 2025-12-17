#include "http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include "parser.h"

static int send_all(int fd, const void *buf, size_t len)
{
    const char *p = (const char *)buf;
    while (len > 0)
    {
        ssize_t n = send(fd, p, len, 0);
        if (n <= 0)
        {
            return -1;
        }
        p += (size_t)n;
        len -= (size_t)n;
    }
    return 0;
}

static int parse_range(const char *request, long file_size, long *out_start, long *out_end)
{
    const char *p = strstr(request, "Range: bytes=");
    if (!p)
    {
        return 0;
    }

    p += (long)strlen("Range: bytes=");

    long start = 0;
    long end = file_size - 1;

    if (*p == '-')
    {
        long suffix = atol(p + 1);
        if (suffix <= 0)
        {
            return -1;
        }
        if (suffix > file_size)
        {
            suffix = file_size;
        }
        start = file_size - suffix;
        end = file_size - 1;
    }
    else
    {
        start = atol(p);
        const char *dash = strchr(p, '-');
        if (!dash)
        {
            return -1;
        }

        if (*(dash + 1) != '\r' && *(dash + 1) != '\n' && *(dash + 1) != '\0')
        {
            end = atol(dash + 1);
        }
    }

    if (start < 0 || start >= file_size)
    {
        return -1;
    }
    if (end < start)
    {
        return -1;
    }
    if (end >= file_size)
    {
        end = file_size - 1;
    }

    *out_start = start;
    *out_end = end;
    return 1;
}

static const char *get_mime_type(const char *path)
{
    const char *ext = strrchr(path, '.');
    if (!ext || ext == path)
    {
        return "application/octet-stream";
    }
    ext++;

    if (strcmp(ext, "html") == 0 || strcmp(ext, "htm") == 0)
    {
        return "text/html; charset=utf-8";
    }
    if (strcmp(ext, "css") == 0)
    {
        return "text/css; charset=utf-8";
    }
    if (strcmp(ext, "js") == 0)
    {
        return "application/javascript";
    }
    if (strcmp(ext, "png") == 0)
    {
        return "image/png";
    }
    if (strcmp(ext, "jpg") == 0 || strcmp(ext, "jpeg") == 0)
    {
        return "image/jpeg";
    }
    if (strcmp(ext, "gif") == 0)
    {
        return "image/gif";
    }
    if (strcmp(ext, "ico") == 0)
    {
        return "image/x-icon";
    }
    if (strcmp(ext, "svg") == 0)
    {
        return "image/svg+xml";
    }
    if (strcmp(ext, "json") == 0)
    {
        return "application/json; charset=utf-8";
    }
    if (strcmp(ext, "txt") == 0)
    {
        return "text/plain; charset=utf-8";
    }
    if (strcmp(ext, "m3u8") == 0)
    {
        return "application/vnd.apple.mpegurl";
    }
    if (strcmp(ext, "ts") == 0)
    {
        return "video/mp2t";
    }
    if (strcmp(ext, "m4s") == 0)
    {
        return "video/iso.segment";
    }
    if (strcmp(ext, "mp4") == 0)
    {
        return "video/mp4";
    }

    return "application/octet-stream";
}

char *not_found_response =
    "HTTP/1.0 404 Not Found\r\n"
    "Content-Type: text/plain\r\n"
    "\r\n"
    "No se encontró el recurso\n";

void handle_client(int client_fd)
{
    char request[1024] = {0};
    char base_path[255] = "./www";
    char buffer[4096];
    FILE *file;
    const char *read_mode = "rb";

    ssize_t req_bytes = read(client_fd, request, sizeof(request) - 1);
    if (req_bytes > 0)
    {
        request[req_bytes] = '\0';
    }
    else
    {
        return;
    }

    char method[10];
    char file_path[255];

    parse_http_request(request, method, file_path);

    if (file_path[0] != '/')
    {
        char tmp[255];
        tmp[0] = '/';
        strncpy(tmp + 1, file_path, sizeof(tmp) - 2);
        tmp[sizeof(tmp) - 1] = '\0';
        strncpy(file_path, tmp, sizeof(file_path) - 1);
        file_path[sizeof(file_path) - 1] = '\0';
    }

    if (strstr(file_path, "..") != NULL)
    {
        write(client_fd, not_found_response, strlen(not_found_response));
        return;
    }

    if (strcmp(file_path, "/") == 0)
    {
        strcpy(file_path, "/index.html");
    }

    char full_path[512];
    snprintf(full_path, sizeof(full_path), "%s%s", base_path, file_path);

    printf("Abriendo archivo: %s\n", full_path);

    file = fopen(full_path, read_mode);

    if (file == NULL)
    {
        perror("Error while opening file");
        write(client_fd, not_found_response, strlen(not_found_response));
        return;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    const char *mime = get_mime_type(file_path);

    long start = 0, end = file_size - 1;
    int has_range = parse_range(request, file_size, &start, &end);
    if (has_range < 0)
    {
        const char *resp =
            "HTTP/1.1 416 Range Not Satisfiable\r\n"
            "Content-Length: 0\r\n"
            "Connection: close\r\n"
            "\r\n";
        send_all(client_fd, resp, strlen(resp));
        fclose(file);
        return;
    }

    long content_length = (has_range == 1) ? (end - start + 1) : file_size;

    char header_buffer[1024];
    int header_len;

    if (has_range == 1)
    {
        header_len = snprintf(header_buffer, sizeof(header_buffer),
                              "HTTP/1.1 206 Partial Content\r\n"
                              "Content-Type: %s\r\n"
                              "Accept-Ranges: bytes\r\n"
                              "Content-Range: bytes %ld-%ld/%ld\r\n"
                              "Content-Length: %ld\r\n"
                              "Connection: close\r\n"
                              "\r\n",
                              mime, start, end, file_size, content_length);

        fseek(file, start, SEEK_SET);
    }
    else
    {
        header_len = snprintf(header_buffer, sizeof(header_buffer),
                              "HTTP/1.1 200 OK\r\n"
                              "Content-Length: %ld\r\n"
                              "Content-Type: %s\r\n"
                              "Accept-Ranges: bytes\r\n"
                              "Connection: close\r\n"
                              "\r\n",
                              file_size, mime);
    }

    if (send_all(client_fd, header_buffer, (size_t)header_len) == -1)
    {
        perror("send headers");
        fclose(file);
        return;
    }

    long remaining = content_length;
    while (remaining > 0)
    {
        size_t to_read = (remaining < (long)sizeof(buffer)) ? (size_t)remaining : sizeof(buffer);
        size_t bytes_read = fread(buffer, 1, to_read, file);
        if (bytes_read == 0)
        {
            break;
        }

        if (send_all(client_fd, buffer, bytes_read) == -1)
        {
            perror("send file");
            break;
        }

        remaining -= (long)bytes_read;
    }

    if (ferror(file))
    {
        perror("Error while reading file");
    }

    fclose(file);
}