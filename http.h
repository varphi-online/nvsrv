#ifndef HTTP_H
#define HTTP_H

#include <netdb.h>
#include <sys/socket.h>

enum http_method {
  GET,
  HEAD,
  POST,
  OPTIONS,
  TRACE,
  PUT,
  DELETE,
  PATCH,
  CONNECT,
  LINK,
  UNLINK
};

extern const char *const http_method_str[];

enum http_version {
  HTTP_0_9,
  HTTP_1_0,
  HTTP_1_1,
  HTTP_2_0,
  HTTP_3_0,
};

extern const char *const http_version_str[];

typedef struct http_request_line {
  enum http_method method;
  char *request_uri;
  enum http_version http_version;
} http_request_line;

/* HTTP */
typedef struct http_header {
  char *key;
  char *value;
  struct http_header *next;
} http_header;

typedef struct http_request_headers {
  struct http_header *head;
  struct http_header *tail;
  size_t size;
  char *_header_buf;
  int _bufsize;
} http_request_headers;

typedef struct http_request {
  http_request_line *request_line;
  http_request_headers *headers;
  char *body;
  int _client_fd;
} http_request;

typedef struct http_server {
  int _socket, _current_accept;
  struct sockaddr_storage _connecting_addr;
  socklen_t _connecting_addr_size;
  struct addrinfo _hints, *res;
  void (*entrypoint)(http_request *, void **);
  int concurrent_connections;
  void **context;
} http_server;

typedef struct http_response {
  int status;
  char *headers;
  char *body;
} http_response;

enum http_status { HTTP_OK = 200, HTTP_NOT_FOUND = 404 };

#define CONTENT_TYPE_TEXT "text/plain"
#define CONTENT_TYPE_JSON "application/json"

http_server http_server_init(char *port,
                             void (*entrypoint)(http_request *, void **),
                             void **context);

// Expects a heap-allocated http_request
// Destroys input string
int http_parse_request(char *request_str, http_request *request);

void http_server_listen(struct http_server server);

int http_respond(struct http_response *response, struct http_request *request);

void http_set_response_status(struct http_response *response, int status);

void http_set_response_header(struct http_response *response, char *key,
                              char *value);
char *http_headers_to_string(struct http_request_headers *headers);

#endif
