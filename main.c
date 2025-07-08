#include "lib/cxl.h"
#include "lib/http.h"
#include "lib/json.h"
#include "lib/sqlite3.h"
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
  json_element *outgoing = *((json_element **)data);
  json_element *current_row = json_obj(0, 0);
  json_append(outgoing, current_row);
  for (int i = 0; i < argc; i++) {
    json_set_key(current_row, azColName[i],
                 argv[i] ? json_str(argv[i]) : json_nul());
  }
  return 0;
}

char *search_course(sqlite3 *db, char *err_msg) {
  json_element *outgoing = json_arr(0);

  char *sql =
      "SELECT * FROM courses WHERE LOWER(department) LIKE \"csc\" LIMIT 15;";
  int rc = sqlite3_exec(db, sql, course_from_row, &outgoing, &err_msg);

  if (rc != SQLITE_OK) {
    fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(db));
    sqlite3_free(err_msg);
    sqlite3_close(db);
    return 0;
  }

  char *out = json_stringify(outgoing, false);
  json_free_element(outgoing);

  return out;
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

  /*
"<p>Lorem ipsum dolor sit amet, consectetur adipisicing elit. Qui dicta minus
molestiae vel beatae natus eveniet ratione temporibus aperiam harum alias
officiis assumenda officia quibusdam deleniti eos cupiditate dolore
doloribus!</p>\
<p>Ad dolore dignissimos asperiores dicta facere optio quod commodi nam tempore
recusandae. Rerum sed nulla eum vero expedita ex delectus voluptates rem at
neque quos facere sequi unde optio aliquam!</p>\
<p>Tenetur quod quidem in voluptatem corporis dolorum dicta sit pariatur porro
quaerat autem ipsam odit quam beatae tempora quibusdam illum! Modi velit odio
nam nulla unde amet odit pariatur at!</p>\
<p>Consequatur rerum amet fuga expedita sunt et tempora saepe? Iusto nihil
explicabo perferendis quos provident delectus ducimus necessitatibus reiciendis
optio tempora unde earum doloremque commodi laudantium ad nulla vel
odio?</p><p>Lorem ipsum dolor sit amet, consectetur adipisicing elit. Qui dicta
minus molestiae vel beatae natus eveniet ratione temporibus aperiam harum alias
officiis assumenda officia quibusdam deleniti eos cupiditate dolore
doloribus!</p>\
<p>Ad dolore dignissimos asperiores dicta facere optio quod commodi nam tempore
recusandae. Rerum sed nulla eum vero expedita ex delectus voluptates rem at
neque quos facere sequi unde optio aliquam!</p>\
<p>Tenetur quod quidem in voluptatem corporis dolorum dicta sit pariatur porro
quaerat autem ipsam odit quam beatae tempora quibusdam illum! Modi velit odio
nam nulla unde amet odit pariatur at!</p>\ <p>Consequatur rerum amet fuga
expedita sunt et tempora saepe? Iusto nihil explicabo perferendis quos provident
delectus ducimus necessitatibus reiciendis optio tempora unde earum doloremque
commodi laudantium ad nulla vel odio?</p>";
*/

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

  /*
  json_element *test_obj =
      json_obj("test", json_str("hello and good fre\b\taki\rn by\4 ae"));
  json_set_key(test_obj, "another one", json_arr(json_str("and a third")));
  json_append(json_get_key(test_obj, "another one"), json_nul());
  json_append(json_get_key(test_obj, "another one"), json_boo(false));
  json_append(json_get_key(test_obj, "another one"), json_num(2));
  char *testo = json_stringify(test_obj, false);
  printf("%s", testo);
  free(testo);
  json_free_element(test_obj);
  */

  struct http_server server = http_server_init("8080", req_handle, context);
  http_server_listen(server);

  /*
  char *result = search_course(db, err_msg);
  printf("%s", result);
  free(result);
*/
  sqlite3_close(db);
  return 1;
}
