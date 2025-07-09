#ifndef JSON_H
#include <stdbool.h>

typedef enum json_value {
  JSON_OBJECT,
  JSON_ARRAY,
  JSON_STRING,
  JSON_NUMBER,
  JSON_BOOLEAN,
  JSON_NULL
} json_value;

// ptr is a context-specific pointer to a struct of json_value type
typedef struct json_element {
  json_value type;
  void *_ptr;
} json_element;

json_element *json_create_element(json_value type);

// Expects a heap-allocated json element, which recursively frees all
// children and self based on internal type automatically
void json_free_element(json_element *element);

// accepts a JSON_OBJECT typed json element
json_element *json_get_key(json_element *object, char *key);

// accepts a JSON_OBJECT typed json element
int json_set_key(json_element *object, char *key, json_element *value);

// accepts a JSON_ARRAY typed json element
void json_append(json_element *array, json_element *to_append);

// pops an element, transferring ownership to the caller
// accepts a JSON_ARRAY typed json element
json_element *json_pop(json_element *array);

// removes the last element of an array silently
// accepts a JSON_ARRAY typed json element
void json_lop(json_element *array);

// accepts a JSON_ARRAY typed json element
json_element *json_get_index(json_element *array, int index);

char *json_stringify(json_element *element, bool pretty_print);

json_element *json_parse(char *input);

// creates a json_element of type JSON_STRING and populates it with the string
// passed
json_element *json_str(char *str);

json_element *json_num(float number);

json_element *json_arr(json_element *first);

json_element *json_nul();

json_element *json_boo(bool value);

json_element *json_obj(char *key, json_element *value);

#endif
