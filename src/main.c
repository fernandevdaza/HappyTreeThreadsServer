#include "server.h"
#include "http.h"
#include <unistd.h>
#include <stdio.h>

int main()
{
    int server_fd = create_socket_and_listen(8080);

    while (1)
    {
        int client_fd = accept_connection(server_fd);
        if (client_fd < 0)
            continue;

        printf("¡Cliente conectado! (FD: %d)\n", client_fd);
        // handle_client(client_fd);
        close(client_fd);
    }
    return 0;
}