#include "http.h"
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

// https://beej.us/guide/bgnet/pdf/bgnet_usl_c_1.pdf

http_request http_parse_request(char *request_str) {
  http_request request = {0};
  request.request_line = strtok(request_str, "\x0d\x0a");
  request.headers = strtok(0, "\x0d\x0a");
  strtok(0, "\x0d\x0a");
  request.body = strtok(0, "\x0d\x0a");

  return request;
}

http_server http_server_init(char *port, void (*entrypoint)(http_request *)) {
  http_server server = {0};

  server.concurrent_connections = 1;

  memset(&server.hints, 0, sizeof server.hints);
  server.hints.ai_family = AF_UNSPEC;     // Any IP v4/v6
  server.hints.ai_socktype = SOCK_STREAM; // TCP
  server.hints.ai_flags = AI_PASSIVE;     // Automatic server IP resoltion

  if (getaddrinfo(NULL, port, &server.hints, &server.res)) {
    fprintf(stderr, "error: getaddrinfo failed\n");
    exit(1);
  }

  server.socket = socket(server.res->ai_family, server.res->ai_socktype,
                         server.res->ai_protocol);

  if (server.socket == -1) {
    perror("socket");
    exit(1);
  }
  int yes = 1;

  setsockopt(server.socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

  if (bind(server.socket, server.res->ai_addr, server.res->ai_addrlen) == -1) {
    perror("bind");
    close(server.socket);
    exit(1);
  }

  if (listen(server.socket, 5) == -1) {
    perror("listen");
    close(server.socket);
    exit(1);
  }
  if (listen(server.socket, 5))
    exit(1);

  server.entrypoint = entrypoint;

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
  char *buf =
      malloc(sizeof(char) * (snprintf(0, 0, "%d", strlen(response->body)) + 2));
  sprintf(buf, "%d", strlen(response->body));

  http_set_response_header(response, "Content-Length", buf);

  char *response_string = http_encode_response(response);
  int len = strlen(response_string);

  int total = 0;
  int bytesleft = len;
  int n;

  while (total < len) {
    n = send(request->client_fd, response_string + total, bytesleft, 0);
    if (n == -1)
      break;
    total += n;
    bytesleft -= n;
  }
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

  pfds[0].fd = server.socket;
  pfds[0].events = POLLIN;

  conn_count = 1;

  printf("waiting for connections...");

  for (;;) {
    int poll_count = poll(pfds, conn_count, -1);

    if (poll_count == -1)
      exit(1);

    for (int i = 0; i < conn_count; i++) {
      if (pfds[i].revents & POLLIN) {
        if (pfds[i].fd == server.socket) {
          struct sockaddr_storage remoteaddr;
          socklen_t addrlen = sizeof remoteaddr;
          int newfd =
              accept(server.socket, (struct sockaddr *)&remoteaddr, &addrlen);
          if (newfd != -1)
            add_to_pfds(&pfds, newfd, &conn_count,
                        &server.concurrent_connections);
        } else {
          int client_fd = pfds[i].fd;
          char buf[2048];
          int nbytes = recv(client_fd, buf, sizeof buf - 1, 0);

          if (nbytes <= 0) {
            close(client_fd);
            del_from_pfds(pfds, i, &conn_count);
          } else {
            buf[nbytes] = '\0';
            http_request request = http_parse_request(buf);
            request.client_fd = client_fd;
            server.entrypoint(&request);
            close(client_fd);
            del_from_pfds(pfds, i, &conn_count);
          }
        }
      }
    }
  }

  close(server.socket);
  freeaddrinfo(&server.hints);
  freeaddrinfo(server.res);
  free(pfds);
}
