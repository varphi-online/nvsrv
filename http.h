#ifndef HTTP_H
#define HTTP_H

#include <netdb.h>
#include <sys/socket.h>

typedef struct http_request {
  char *request_line;
  char *headers;
  char *body;
  int client_fd;
} http_request;

typedef struct http_server {
  int socket, current_accept;
  struct sockaddr_storage connecting_addr;
  socklen_t connecting_addr_size;
  struct addrinfo hints, *res;
  void (*entrypoint)(http_request *);
  int concurrent_connections;
} http_server;

typedef struct http_response {
  int status;
  char *headers;
  char *body;
} http_response;

enum http_status { HTTP_OK = 200, HTTP_NOT_FOUND = 404 };

#define CONTENT_TYPE_TEXT "text/plain"
#define CONTENT_TYPE_JSON "application/json"

http_server http_server_init(char *port, void (*entrypoint)(http_request *));

http_request http_parse_request(char *request_str);

void http_server_listen(struct http_server server);

int http_respond(struct http_response *response, struct http_request *request);

void http_set_response_status(struct http_response *response, int status);

void http_set_response_header(struct http_response *response, char *key,
                              char *value);

#endif
