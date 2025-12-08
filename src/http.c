#include "http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include "parser.h"

const char *headers =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/html; charset=UTF-8\r\n"
    "\r\n";

char *default_response =
    "HTTP/1.0 200 OK\r\n"
    "Content-Type: text/plain\r\n"
    "\r\n"
    "Hola! Soy un servidor HTTP real.\n";

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
        printf("\n--- Request recibido ---\n%s\n------------------------\n", request);
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

    if (send(client_fd, headers, strlen(headers), 0) == -1)
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
