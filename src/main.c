#include "server.h"
#include "http.h"
#include <unistd.h>
#include <stdio.h>

int main()
{
    init_thread_pool();
    int server_fd = create_socket_and_listen(8000);

    printf("Servidor HTTP escuchando en puerto 8000...\n");

    while (1)
    {
        int client_fd = accept_connection(server_fd);
        if (client_fd < 0)
            continue;

        enqueue_client(client_fd);
    }
    return 0;
}