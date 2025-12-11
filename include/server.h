#ifndef SERVER_H
#define SERVER_H

int create_socket_and_listen(int port);

int accept_connection(int listen_fd);

void init_thread_pool(void);

void enqueue_client(int client_fd);

#endif