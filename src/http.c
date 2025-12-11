#include "http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include "parser.h"

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
    char header_buffer[1024];
    int header_len = snprintf(header_buffer, sizeof(header_buffer),
                              "HTTP/1.1 200 OK\r\n"
                              "Content-Length: %ld\r\n"
                              "Content-Type: %s\r\n"
                              "\r\n",
                              file_size, mime);

    if (send(client_fd, header_buffer, header_len, 0) == -1)
    {
        perror("send headers");
        fclose(file);
        return;
    }

    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0)
    {
        ssize_t bytes_sent = send(client_fd, buffer, bytes_read, 0);
        if (bytes_sent == -1)
        {
            perror("Error while send file chunk");
            break;
        }
    }

    if (ferror(file))
    {
        perror("Error while reading file");
    }

    fclose(file);
}
