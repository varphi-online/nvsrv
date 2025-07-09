#include "http.h"
#include <ctype.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

const char *const http_method_str[] = {
    [GET] = "GET",         [HEAD] = "HEAD",    [POST] = "POST",
    [OPTIONS] = "OPTIONS", [TRACE] = "TRACE",  [PUT] = "PUT",
    [DELETE] = "DELETE",   [PATCH] = "PATCH",  [CONNECT] = "CONNECT",
    [LINK] = "LINK",       [UNLINK] = "UNLINK"};

const char *const http_version_str[] = {
    [HTTP_0_9] = "HTTP/0.9", [HTTP_1_0] = "HTTP/1.0", [HTTP_1_1] = "HTTP/1.1",
    [HTTP_2_0] = "HTTP/2.0", [HTTP_3_0] = "HTTP/3.0",
};

// https://beej.us/guide/bgnet/pdf/bgnet_usl_c_1.pdf

// strips whitespace and shifts the body of the string to the
// front of memory allocated to it
void strip_whitespace(char *str) {
  if (str == NULL || *str == '\0') {
    return;
  }
  char *start = str;
  while (isspace((unsigned char)*start)) {
    start++;
  }
  if (*start == '\0') {
    *str = '\0';
    return;
  }
  char *end = start + strlen(start) - 1;
  while (end > start && isspace((unsigned char)*end)) {
    end--;
  }
  *(end + 1) = '\0';
  if (str != start) {
    memmove(str, start, end - start + 2);
  }
}

void free_http_request(http_request *request) {
  if (request == NULL) {
    return;
  }
  free(request->request_line);
  if (request->headers != NULL) {
    // All pointers from all headers are into one contigous
    // block, which we can free all at once
    if (request->headers->_header_buf != NULL) {
      free(request->headers->_header_buf);
    }
    http_header *current = request->headers->head;
    while (current != NULL) {
      http_header *next_node = current->next;
      free(current);
      current = next_node;
    }
    free(request->headers);
  }
  free(request);
}

int http_parse_request(char *request_str, http_request *request) {
  http_request_line *request_line = malloc(sizeof(http_request_line));

  http_request_headers *headers = malloc(sizeof(http_request_headers));
  // defer allocating the body until we have a Content-Length header
  // or something else
  request->request_line = request_line;
  request->headers = headers;

  // Begin RL parse -------

  char *unparsed_rql = strtok(request_str, "\r\n");
  request_str = strtok(NULL, "\0"); // save rest of request str for later

  char *undef_method = strtok(unparsed_rql, " ");
  bool method_found = false;
  for (int i = 0; i < (sizeof(http_method_str) / sizeof(http_method_str[0]));
       i++) {
    if (strcmp(undef_method, http_method_str[i]) == 0) {
      request_line->method = (enum http_method)i;
      method_found = true;
      break;
    }
  }

  if (!method_found) {
    puts("Invalid request method");
    return -1;
  }

  request_line->request_uri = strtok(0, " ");

  char *undef_vers = strtok(NULL, "\0");
  bool version_found = false;
  for (int i = 0; i < (sizeof(http_version_str) / sizeof(http_version_str[0]));
       i++) {
    if (strcmp(undef_vers, http_version_str[i]) == 0) {
      request_line->http_version = (enum http_version)i;
      version_found = true;
      break;
    }
  }

  if (!version_found) {
    puts("Invalid request HTTP version");
    return -1;
  }

  // End of RL parse --------

  // Begin HEAD parse -------

  /* General idea is allocate size equal to the string that contains ALL
   * headers, then, each http_header in the http_request_headers linked-list
   * will point to a segment of that allocated memory, with each segment being a
   * null-terminated string. Essentially:
   *
   *  {key,value,next}  {key,value,next}
   *    ╱   │      └────⬏
   *   ╱    │
   *  ↙     ↓
   * "Host\0localhost:8080\0..."   // Post-Parse
   * "Host: localhost:8080\r\n..." // Original Malloc/Copy
   *
   * Via RFC 1945 (HTTP 1.0):
   * HTTP-header    = field-name ":" [ field-value ] CRLF
   */

  char *end_of_headers = strstr(
      request_str, "\r\n\r\n"); // empty CRLF line indicates end of headers
  if (end_of_headers == NULL) {
    return -1;
  }
  size_t headers_total_size = end_of_headers - request_str + 1;
  headers->_bufsize = headers_total_size;

  char *header_buf = malloc(headers_total_size * sizeof(char));
  headers->_header_buf = header_buf;
  memcpy(header_buf, request_str, headers_total_size);

  request->body = end_of_headers + 4; // new unparsed str is body only

  char *token = strtok(header_buf, ":"); // get key split
  strip_whitespace(token);

  http_header *first = malloc(sizeof(http_header));
  first->key = token;
  first->next = NULL;
  headers->head = first;
  headers->tail = first;
  headers->size = 1;

  for (;;) {
    headers->tail->value = strtok(0, "\r\n");
    strip_whitespace(headers->tail->value);
    if (!token)
      break;
    token = strtok(NULL, ":"); // get key of current
    strip_whitespace(token);
    if (!token)
      break;
    http_header *current =
        malloc(sizeof(http_header)); // create current with key
    current->key = token;
    current->next = NULL;
    headers->tail->next = current; // link struct
    headers->tail = current;       // readdress tail
    headers->size += 1;
  };

  // End HEAD parse ---------

  return 0;
}

char *http_headers_to_string(struct http_request_headers *headers,
                             bool pretty_print) {
  int offset = 0;
  char *out = malloc(headers->_bufsize * sizeof(char));
  http_header *current = headers->head;
  while (current != NULL) {
    offset += snprintf(out + offset, headers->_bufsize - offset,
                       pretty_print ? "%s: %s\n" : "{%s:%s},", current->key,
                       current->value);
    current = current->next;
  }
  return out;
}

http_server http_server_init(char *port,
                             void (*entrypoint)(http_request *, void **),
                             void **context) {
  http_server server = {0};

  server.concurrent_connections = 1;

  memset(&server._hints, 0, sizeof server._hints);
  server._hints.ai_family = AF_UNSPEC;     // Any IP v4/v6
  server._hints.ai_socktype = SOCK_STREAM; // TCP
  server._hints.ai_flags = AI_PASSIVE;     // Automatic server IP resoltion

  if (getaddrinfo(NULL, port, &server._hints, &server.res)) {
    fprintf(stderr, "error: getaddrinfo failed\n");
    exit(1);
  }

  server._socket = socket(server.res->ai_family, server.res->ai_socktype,
                          server.res->ai_protocol);

  if (server._socket == -1) {
    perror("_socket");
    exit(1);
  }
  int yes = 1;

  setsockopt(server._socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

  if (bind(server._socket, server.res->ai_addr, server.res->ai_addrlen) == -1) {
    perror("bind");
    close(server._socket);
    exit(1);
  }

  if (listen(server._socket, 5) == -1) {
    perror("listen");
    close(server._socket);
    exit(1);
  }
  if (listen(server._socket, 5))
    exit(1);

  server.entrypoint = entrypoint;
  server.context = context;

  return server;
}

void add_to_pfds(struct pollfd **pfds, int newfd, int *fd_count, int *fd_size) {
  if (*fd_count == *fd_size) {
    *fd_size *= 2;
    *pfds = realloc(*pfds, sizeof(struct pollfd) * (*fd_size));
    if (*pfds == NULL) {
      perror("realloc");
      exit(1);
    }
  }

  (*pfds)[*fd_count].fd = newfd;
  (*pfds)[*fd_count].events = POLLIN;

  (*fd_count)++;
}

void del_from_pfds(struct pollfd pfds[], int i, int *fd_count) {
  pfds[i] = pfds[*fd_count - 1];
  (*fd_count)--;
}

char *http_encode_response(struct http_response *response) {
  const char *headers = response->headers ? response->headers : "";
  const char *body = response->body ? response->body : "";
  int len = 0;
  char *str = NULL;

  len = snprintf(NULL, 0, "HTTP/1.1 %d\x0d\x0a%s\x0d\x0a%s", response->status,
                 headers, body);
  str = malloc(len + 1);
  if (str == NULL) {
    perror("malloc");
    return NULL;
  }
  snprintf(str, len + 1, "HTTP/1.1 %d\x0d\x0a%s\x0d\x0a%s", response->status,
           headers, body);
  return str;
}
int http_respond(struct http_response *response, struct http_request *request) {
  if (response->body == NULL)
    return -1;
  char *buf = malloc(sizeof(char) *
                     (snprintf(0, 0, "%ld", strlen(response->body)) + 2));
  sprintf(buf, "%ld", strlen(response->body));

  http_set_response_header(response, "Content-Length", buf);

  char *response_string = http_encode_response(response);

  int len = strlen(response_string);

  int total = 0;
  int bytesleft = len;
  int n;

  while (total < len) {
    n = send(request->_client_fd, response_string + total, bytesleft, 0);
    if (n == -1)
      break;
    total += n;
    bytesleft -= n;
  }
  free_http_request(request);
  free(response_string);
  return n == -1 ? -1 : 0;
}

void http_set_response_status(struct http_response *response, int status) {
  response->status = status;
}

void http_set_response_header(struct http_response *response, char *key,
                              char *value) {
  size_t new_line_len = snprintf(NULL, 0, "%s: %s\r\n", key, value);

  size_t old_headers_len = response->headers ? strlen(response->headers) : 0;

  char *new_headers =
      realloc(response->headers, old_headers_len + new_line_len + 1);
  if (new_headers == NULL) {
    perror("realloc");
    return;
  }

  sprintf(new_headers + old_headers_len, "%s: %s\r\n", key, value);

  response->headers = new_headers;
}

void http_server_listen(struct http_server server) {
  int conn_count = 0;
  struct pollfd *pfds = malloc(sizeof *pfds * server.concurrent_connections);

  pfds[0].fd = server._socket;
  pfds[0].events = POLLIN;

  conn_count = 1;

  for (;;) {
    int poll_count = poll(pfds, conn_count, -1);

    if (poll_count == -1)
      exit(1);

    for (int i = 0; i < conn_count; i++) {
      if (pfds[i].revents & POLLIN) {
        if (pfds[i].fd == server._socket) {
          struct sockaddr_storage remoteaddr;
          socklen_t addrlen = sizeof remoteaddr;
          int newfd =
              accept(server._socket, (struct sockaddr *)&remoteaddr, &addrlen);
          if (newfd != -1)
            add_to_pfds(&pfds, newfd, &conn_count,
                        &server.concurrent_connections);
        } else {
          int _client_fd = pfds[i].fd;
          char buf[2048];
          int nbytes = recv(_client_fd, buf, sizeof buf - 1, 0);

          if (nbytes <= 0) {
            close(_client_fd);
            del_from_pfds(pfds, i, &conn_count);
          } else {
            buf[nbytes] = '\0';
            http_request *request = calloc(1, sizeof(http_request));

            if (http_parse_request(buf, request) != 0) {
              fprintf(stderr, "Failed to parse HTTP request.\n");
              free(request);
            } else {
              request->_client_fd = _client_fd;
              server.entrypoint(request, server.context);
            }

            close(_client_fd);
            del_from_pfds(pfds, i, &conn_count);
          }
        }
      }
    }
  }

  close(server._socket);
  freeaddrinfo(&server._hints);
  freeaddrinfo(server.res);
  free(pfds);
}
