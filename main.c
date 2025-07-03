#include "cxl.h"
#include "http.h"
#include "sqlite3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  char **courses;
  size_t count;
  size_t capacity;
} CourseList;

// Outputs a json object representing a course
int course_from_row(void *data, int argc, char **argv, char **azColName) {
  CourseList *courselist = (CourseList *)data;

  size_t totalLen = 3; // "{}\n"
  for (int i = 0; i < argc; i++) {
    totalLen +=
        strlen(azColName[i]) + strlen(argv[i] ? argv[i] : "null") +
        7; // includes space for four quotes, a colon, a comma and nullterm
  }
  char *buffer = malloc(totalLen);
  if (!buffer)
    return 0;

  int offset = 1;
  buffer[0] = '{';

  for (int i = 0; i < argc; i++) {
    // BUG: in scraper or here, need to remove <br /> tags (specifically in the
    // attributes field) as unescaped html tags are not RFC 8259 JSON compliant
    // workaround is skipping attributes field

    char *format = argv[i] ? "\"%s\": \"%s\"" : "\"%s\": %s";
    if (strcmp(azColName[i], "attributes"))
      offset += sprintf(buffer + offset, format, azColName[i],
                        argv[i] ? argv[i] : "null");
    if (i < argc - 2) {
      offset += sprintf(buffer + offset, ",");
    }
  }
  sprintf(buffer + offset, "}");

  // Aggregating rows into courselist

  if (courselist->count >= courselist->capacity) {
    courselist->courses =
        realloc(courselist->courses, courselist->capacity * 2 * sizeof(char *));
    courselist->capacity *= 2;
  }

  courselist->courses[courselist->count] = buffer;
  courselist->count += 1;

  return 0;
}

char *search_course(sqlite3 *db, char *err_msg) {
  // aggregate rows for usage
  size_t initial_capacity = 100;
  CourseList courselist;
  courselist.courses = malloc(initial_capacity * sizeof(char *));
  if (!courselist.courses) {
    fprintf(stderr, "Initial malloc failed\n");
    sqlite3_close(db);
    return 0;
  }
  courselist.count = 0;
  courselist.capacity = initial_capacity;

  char *sql = "SELECT * FROM courses WHERE LOWER(department) LIKE \"csc\";";
  int rc = sqlite3_exec(db, sql, course_from_row, &courselist, &err_msg);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(db));

    sqlite3_free(err_msg);
    sqlite3_close(db);
    return 0;
  }

  // Join strings into one str object
  size_t allocSize = 0;
  for (int i = 0; i < courselist.count; i++) {
    allocSize += strlen(courselist.courses[i]);
  }

  char *out = malloc(sizeof(char) * (allocSize + courselist.count + 4));
  out[0] = '[';

  cxl_join_with_delim(out + 1, courselist.courses, courselist.count, ",");

  for (int i = 0; i < courselist.count; i++) {
    free(courselist.courses[i]);
  }
  free(courselist.courses);

  return strcat(out, "]");
}

void req_handle(http_request *request, void **context) {
  puts("recieved");
  printf("------------\n"
         "Request-Line: \n"
         "\t%s %s %s\n\n"
         "Headers: \n"
         "%s\n"
         "Body: \n"
         "\t%s\n"
         "------------\n",
         http_method_str[request->request_line->method],
         request->request_line->request_uri,
         http_version_str[request->request_line->http_version],
         http_headers_to_string(request->headers), request->body);

  http_response response = {0};
  if (!strstr(request->request_line->request_uri, "/api/course_search")) {
    response.body = search_course((sqlite3 *)context[0], (char *)context[1]);
  }

  http_set_response_status(&response, response.body ? HTTP_OK : HTTP_NOT_FOUND);
  http_set_response_header(&response, "Content-Type", CONTENT_TYPE_TEXT);

  "<p>Lorem ipsum dolor sit amet, consectetur adipisicing elit. Qui dicta minus molestiae vel beatae natus eveniet ratione temporibus aperiam harum alias officiis assumenda officia quibusdam deleniti eos cupiditate dolore doloribus!</p>\
<p>Ad dolore dignissimos asperiores dicta facere optio quod commodi nam tempore recusandae. Rerum sed nulla eum vero expedita ex delectus voluptates rem at neque quos facere sequi unde optio aliquam!</p>\
<p>Tenetur quod quidem in voluptatem corporis dolorum dicta sit pariatur porro quaerat autem ipsam odit quam beatae tempora quibusdam illum! Modi velit odio nam nulla unde amet odit pariatur at!</p>\
<p>Consequatur rerum amet fuga expedita sunt et tempora saepe? Iusto nihil explicabo perferendis quos provident delectus ducimus necessitatibus reiciendis optio tempora unde earum doloremque commodi laudantium ad nulla vel odio?</p><p>Lorem ipsum dolor sit amet, consectetur adipisicing elit. Qui dicta minus molestiae vel beatae natus eveniet ratione temporibus aperiam harum alias officiis assumenda officia quibusdam deleniti eos cupiditate dolore doloribus!</p>\
<p>Ad dolore dignissimos asperiores dicta facere optio quod commodi nam tempore recusandae. Rerum sed nulla eum vero expedita ex delectus voluptates rem at neque quos facere sequi unde optio aliquam!</p>\
<p>Tenetur quod quidem in voluptatem corporis dolorum dicta sit pariatur porro quaerat autem ipsam odit quam beatae tempora quibusdam illum! Modi velit odio nam nulla unde amet odit pariatur at!</p>\
<p>Consequatur rerum amet fuga expedita sunt et tempora saepe? Iusto nihil explicabo perferendis quos provident delectus ducimus necessitatibus reiciendis optio tempora unde earum doloremque commodi laudantium ad nulla vel odio?</p>";

  http_respond(&response, request);
  free(response.body);
}

int main() {
  // SQLITE Initializtion
  sqlite3 *db;
  char *err_msg = 0;

  int rc = sqlite3_open("catalog.db", &db);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Cannot open database %s\n", sqlite3_errmsg(db));
    sqlite3_close(db);

    return 1;
  }

  void *context[2];
  context[0] = db;
  context[1] = err_msg;

  struct http_server server = http_server_init("8080", req_handle, context);
  http_server_listen(server);

  char *result = search_course(db, err_msg);
  printf("%s", result);
  free(result);

  sqlite3_close(db);
  return 1;
}
