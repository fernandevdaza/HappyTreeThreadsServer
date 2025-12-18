#include "parser.h"
#include <stdio.h>
#include <string.h>
#include "logger.h"

void sanitize_path(char *path)
{

    if (strcmp(path, "/") == 0)
    {
        strcpy(path, "/index.html");
    }
}

void parse_http_request(char *request, char *method, char *file_path)
{

    int parsed = sscanf(request, "%s %s", method, file_path);

    if (parsed < 2)
    {
        printf("Error: Petición HTTP mal formada");
        return;
    }

    if (!is_logging_enabled()) {
        printf("Método detectado: %s\n", method);
        printf("Archivo solicitado: %s\n", file_path);
    }

    sanitize_path(file_path);
}