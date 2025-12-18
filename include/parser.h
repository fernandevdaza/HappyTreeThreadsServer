#ifndef PARSER_H
#define PARSER_H

void sanitize_path(char *path);

void parse_http_request(char *request, char *method, char *file_path);

#endif
