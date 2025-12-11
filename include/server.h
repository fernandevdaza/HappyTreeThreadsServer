#ifndef SERVER_H
#define SERVER_H

int create_socket_and_listen(int port);

int accept_connection(int listen_fd);

// Inicializa la cola y los hilos workers
void init_thread_pool(void);

// Agrega un cliente a la cola para ser procesado por un worker
void enqueue_client(int client_fd);


#endif