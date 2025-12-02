#include "http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>

void handle_client(int client_fd)
{
    char buffer[1024] = {0};

    // 1. LEER la petición del navegador/curl
    // (Por ahora solo la leemos para vaciar el buffer, no la analizamos aún)
    read(client_fd, buffer, 1024);
    printf("\n");
    printf(buffer);
    printf("--- Request recibido ---\n%s\n------------------------\n", buffer);

    // 2. PREPARAR la respuesta HTTP Estándar
    // Header básico: Protocolo + Código (200 OK) + Tipo de contenido + Separador (\r\n\r\n)
    char *response =
        "HTTP/1.0 200 OK\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        "Hola! Soy un servidor HTTP real.\n";

    // 3. ENVIAR respuesta
    write(client_fd, response, strlen(response));

    // El socket se cerrará en el main o aquí, dependiendo de tu diseño.
    // Como es HTTP/1.0 no persistente, lo cerramos tras enviar.
}