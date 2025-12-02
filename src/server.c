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