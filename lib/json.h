#ifndef JSON_H

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

// A trie of member keys whos leaf nodes point to the element of the member
struct json_object {};

json_element *json_get_key(json_element *object, char *key);

int json_set_key(json_element *object, char *key, json_element *value);

// Expects a heap-allocated json element, which recursively frees all
// children and self based on internal type automatically
void json_free_element(json_element *element);

#endif
