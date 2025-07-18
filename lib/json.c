#include "json.h"
#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define JSON_ARRAY_START_SIZE 10

typedef struct _json_kv_entry {
  json_element *key;
  json_element *value;
  struct _json_kv_entry *next;
} _json_kv_entry;

// Frees heap allocated kv entry and its children
static void _json_kv_free(_json_kv_entry *kv) {
  json_free_element(kv->key);
  json_free_element(kv->value);
}

json_element **_json_kv_insert(_json_kv_entry **root, char *key) {
  if (*root == NULL) {
    *root = calloc(1, sizeof(_json_kv_entry));
    (*root)->key = json_str(key);
    return &(*root)->value;
  }
  _json_kv_entry *current = *root;
  while (current->next) {
    current = current->next;
  }
  current->next = calloc(1, sizeof(_json_kv_entry));
  current->next->key = json_str(key);
  return &current->next->value;
}

json_element *_json_kv_search(_json_kv_entry *root, char *key) {
  _json_kv_entry *current = root;
  while (current) {
    if (strcmp(current->key->_ptr, key) == 0)
      return current->value;
    current = current->next;
  }
  return NULL;
}

void _json_kv_delete(_json_kv_entry **root, char *key) {
  _json_kv_entry *current = *root;
  _json_kv_entry *prev = NULL;

  while (current) {
    if (strcmp(current->key->_ptr, key) == 0) {
      if (prev) {
        prev->next = current->next;
      } else {
        *root = current->next;
      }
      _json_kv_free(current);
      free(current);
      return;
    }
    prev = current;
    current = current->next;
  }
}

int json_set_key(json_element *object, char *key, json_element *value) {

  if (object->type != JSON_OBJECT)
    return -1;

  // Passing a nullptr to set removes kv from trie
  if (value == NULL) {
    _json_kv_delete((_json_kv_entry **)&object->_ptr, key);
    return 0;
  }

  _json_kv_entry *current = object->_ptr;
  while (current != NULL) {
    if (strcmp(current->key->_ptr, key) == 0) {
      json_free_element(current->value);
      current->value = value;
      return 0;
    }
    current = current->next;
  }

  json_element **loc = _json_kv_insert((_json_kv_entry **)&object->_ptr, key);

  if (!loc)
    return -1;
  *loc = value;
  return 0;
}

json_element *json_get_key(json_element *object, char *key) {
  if (object->type != JSON_OBJECT || object->_ptr == NULL)
    return NULL;
  return _json_kv_search(object->_ptr, key);
}

typedef struct _json_array_internal {
  size_t capacity;
  size_t count;
  json_element **head;
} _json_array_internal;

void json_append(json_element *array, json_element *to_append) {
  if (array->type != JSON_ARRAY || to_append == NULL)
    return;
  _json_array_internal **internal_ptr = (_json_array_internal **)&array->_ptr;
  if (*internal_ptr == NULL) {
    *internal_ptr = calloc(1, sizeof(_json_array_internal));
    (*internal_ptr)->head =
        calloc(JSON_ARRAY_START_SIZE, sizeof(json_element *));
    (*internal_ptr)->capacity = JSON_ARRAY_START_SIZE;
  }

  _json_array_internal *internal = *internal_ptr;
  if (internal->count > internal->capacity - 1) {
    internal->head = realloc(internal->head,
                             internal->capacity * 2 * sizeof(json_element *));
    internal->capacity *= 2;
  }
  internal->head[internal->count] = to_append;
  internal->count++;
};

json_element *json_pop(json_element *array) {
  if (array == NULL || array->type != JSON_ARRAY)
    return NULL;
  _json_array_internal **ip = (_json_array_internal **)&array->_ptr;
  if (*ip == NULL)
    return NULL;
  _json_array_internal *internal = *ip;
  if (internal->count == 0) {
    return NULL;
  }
  internal->count--;
  json_element *out = internal->head[internal->count];
  internal->head[internal->count] = NULL;
  return out;
};

json_element *json_get_index(json_element *array, int index) {
  if (array == NULL || array->type != JSON_ARRAY)
    return NULL;
  _json_array_internal **ip = (_json_array_internal **)&array->_ptr;
  if (*ip == NULL)
    return NULL;
  _json_array_internal *internal = *ip;
  return internal->head[index];
}

void json_free_element(json_element *element) {
  if (element == NULL)
    return;
  switch (element->type) {
  case JSON_OBJECT:
    if (element->_ptr) {
      _json_kv_entry *current = element->_ptr;
      _json_kv_entry *next;
      while (current != NULL) {
        next = current->next;
        _json_kv_free(current);
        free(current);
        current = next;
      }
    }
    break;
  case JSON_ARRAY:
    if (!element->_ptr)
      break;
    _json_array_internal *internal = (_json_array_internal *)element->_ptr;
    for (size_t i = 0; i < internal->count; i++) {
      json_free_element(internal->head[i]);
    }
    free(internal->head);
    free(element->_ptr);
    break;
  case JSON_STRING:
    free(element->_ptr);
    break;
  case JSON_BOOLEAN:
    free(element->_ptr);
    break;
  case JSON_NUMBER:
    free(element->_ptr);
    break;
  case JSON_NULL:
    break;
  }
  free(element);
}

inline json_element *json_create_element(json_value type) {
  json_element *out = calloc(1, sizeof(json_element));
  out->type = type;
  return out;
};

json_element *json_str(char *str) {
  json_element *out = json_create_element(JSON_STRING);
  out->_ptr = strdup(str);
  return out;
}

static void skip_whitespace(char **c) {
  while (isspace(**c))
    (*c)++;
}

static json_element *parse_str(char **cursor_ptr) {
  size_t i = 0, size = 256;
  char *buf = malloc(size);
  if (!buf)
    return NULL;
  while (**cursor_ptr != '\"' && **cursor_ptr) {
    if (i >= size - 1)
      if (!(buf = realloc(buf, size *= 2)))
        return NULL;
    if (**cursor_ptr == '\\') {
      (*cursor_ptr)++;
      switch (*(*cursor_ptr)++) {
      case '\"':
        buf[i++] = '\"';
        break;
      case '\\':
        buf[i++] = '\\';
        break;
      case 'b':
        buf[i++] = '\b';
        break;
      case 'f':
        buf[i++] = '\f';
        break;
      case 'n':
        buf[i++] = '\n';
        break;
      case 'r':
        buf[i++] = '\r';
        break;
      case 't':
        buf[i++] = '\t';
        break;
      default:
        buf[i++] = *(*cursor_ptr - 1);
        break;
      }
    } else {
      buf[i++] = *(*cursor_ptr)++;
    }
  }
  buf[i] = '\0';
  json_element *out = json_str(buf);
  free(buf);
  if (**cursor_ptr == '\"')
    (*cursor_ptr)++;
  return out;
}

json_element *json_parse_element(char **cursor_ptr);

static json_element *parse_arr(char **cursor_ptr) {
  json_element *out = json_create_element(JSON_ARRAY);
  if (!out)
    return NULL;
  (*cursor_ptr)++;
  skip_whitespace(cursor_ptr);
  if (**cursor_ptr != ']') {
    for (;;) {
      json_append(out, json_parse_element(cursor_ptr));
      skip_whitespace(cursor_ptr);
      if (**cursor_ptr != ',')
        break;
      (*cursor_ptr)++;
      skip_whitespace(cursor_ptr);
    }
  }
  if (**cursor_ptr == ']') {
    (*cursor_ptr)++;
    return out;
  }
  json_free_element(out);
  return json_nul();
}

static json_element *parse_obj(char **cursor_ptr) {
  json_element *out = json_create_element(JSON_OBJECT);
  if (!out)
    return NULL;
  (*cursor_ptr)++;
  skip_whitespace(cursor_ptr);
  if (**cursor_ptr != '}') {
    for (;;) {
      if (*(*cursor_ptr)++ != '"')
        break;
      json_element *key = parse_str(cursor_ptr);
      if (!key)
        break;
      skip_whitespace(cursor_ptr);
      if (*(*cursor_ptr)++ != ':') {
        json_free_element(key);
        break;
      }
      skip_whitespace(cursor_ptr);
      json_set_key(out, key->_ptr, json_parse_element(cursor_ptr));
      json_free_element(key);
      skip_whitespace(cursor_ptr);
      if (**cursor_ptr != ',')
        break;
      (*cursor_ptr)++;
      skip_whitespace(cursor_ptr);
    }
  }
  if (**cursor_ptr == '}') {
    (*cursor_ptr)++;
    return out;
  }
  json_free_element(out);
  return json_nul();
}

json_element *json_parse_element(char **cursor_ptr) {
  skip_whitespace(cursor_ptr);
  char cur = **cursor_ptr;
  if (cur == '"') {
    (*cursor_ptr)++;
    return parse_str(cursor_ptr);
  }
  if (cur == '[')
    return parse_arr(cursor_ptr);
  if (cur == '{')
    return parse_obj(cursor_ptr);
  if (cur == 't' && !strncmp(*cursor_ptr, "true", 4)) {
    *cursor_ptr += 4;
    return json_boo(true);
  }
  if (cur == 'f' && !strncmp(*cursor_ptr, "false", 5)) {
    *cursor_ptr += 5;
    return json_boo(false);
  }
  if (cur == 'n' && !strncmp(*cursor_ptr, "null", 4)) {
    *cursor_ptr += 4;
    return json_nul();
  }
  if (isdigit(cur) || cur == '-') {
    char *end;
    float val = strtof(*cursor_ptr, &end);
    if (end != *cursor_ptr) {
      *cursor_ptr = end;
      return json_num(val);
    }
  }
  return json_nul();
}

json_element *json_parse(char *input) { return json_parse_element(&input); }

void _json_stringify_internal(json_element *element, bool pretty_print,
                              FILE *buffer) {
  if (element == NULL) {
    return;
  }

  switch (element->type) {
  case JSON_STRING:
    fputc('"', buffer);
    if (element->_ptr) {
      char *p = element->_ptr;
      while (*p) {
        unsigned char c = *p;
        switch (c) {
        case '"':
          fputs("\\\"", buffer);
          break;
        case '\\':
          fputs("\\\\", buffer);
          break;
        case '/':
          fputs("\\/", buffer);
          break;
        case '\b':
          fputs("\\b", buffer);
          break;
        case '\f':
          fputs("\\f", buffer);
          break;
        case '\n':
          fputs("\\n", buffer);
          break;
        case '\r':
          fputs("\\r", buffer);
          break;
        case '\t':
          fputs("\\t", buffer);
          break;
        default:
          if (c < 0x20) {
            fprintf(buffer, "\\u%04x", c);
          } else {
            fputc(c, buffer);
          }
          break;
        }
        p++;
      }
    }
    fputc('"', buffer);
    break;

  case JSON_ARRAY:
    fputc('[', buffer);
    if (element->_ptr) {
      _json_array_internal *internal = (_json_array_internal *)element->_ptr;
      if (internal->count > 0) {
        _json_stringify_internal(internal->head[0], pretty_print, buffer);
        for (size_t i = 1; i < internal->count; i++) {
          fputc(',', buffer);
          _json_stringify_internal(internal->head[i], pretty_print, buffer);
        }
      }
    }
    fputc(']', buffer);
    break;

  case JSON_OBJECT:
    fputc('{', buffer);
    if (element->_ptr) {
      _json_kv_entry *current = element->_ptr;
      bool first = true;
      while (current) {
        if (!first) {
          fputc(',', buffer);
        }
        _json_stringify_internal(current->key, pretty_print, buffer);
        fputc(':', buffer);
        _json_stringify_internal(current->value, pretty_print, buffer);
        first = false;
        current = current->next;
      }
    }
    fputc('}', buffer);
    break;

  case JSON_NUMBER:
    if (element->_ptr) {
      fprintf(buffer, "%f", *((float *)element->_ptr));
    } else {
      fputs("null", buffer);
    }
    break;

  case JSON_BOOLEAN:
    if (element->_ptr) {
      fputs(*((bool *)element->_ptr) ? "true" : "false", buffer);
    } else {
      fputs("null", buffer);
    }
    break;

  case JSON_NULL:
    fputs("null", buffer);
    break;
  }
}

char *json_stringify(json_element *element, bool pretty_print) {
  char *buffer;
  size_t size;
  FILE *stream = open_memstream(&buffer, &size);
  if (!stream) {
    return NULL;
  }
  _json_stringify_internal(element, pretty_print, stream);
  fclose(stream);
  return buffer;
}

json_element *json_num(float number) {
  json_element *out = json_create_element(JSON_NUMBER);
  out->_ptr = (float *)malloc(sizeof(float));
  *((float *)out->_ptr) = number;
  return out;
}

json_element *json_arr(json_element *first) {
  json_element *out = json_create_element(JSON_ARRAY);
  json_append(out, first);
  return out;
}

json_element *json_nul() {
  json_element *out = json_create_element(JSON_NULL);
  return out;
}

json_element *json_boo(bool value) {
  json_element *out = json_create_element(JSON_BOOLEAN);
  out->_ptr = (bool *)malloc(sizeof(bool));
  *((bool *)out->_ptr) = value;
  return out;
}

json_element *json_obj(char *key, json_element *value) {
  json_element *out = json_create_element(JSON_OBJECT);
  json_set_key(out, key, value);
  return out;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
 typedef struct _member_tree_node {
  struct _member_tree_node *left;
  struct _member_tree_node *right;
  const char *key;
  json_element *value;
} _member_tree_node;

json_element **_json_member_tree_insert(_member_tree_node **root,
                                        const char *key) {

  _member_tree_node **current = root;
  while (*current) {
    int comparison = strcmp((*current)->key, key);
    if (comparison == 0)
    fputc('[', buffer);
      return &(*current)->value;
    current = (comparison > 0) ? &(*current)->left : &(*current)->right;
  }

  if (!(*current = calloc(1, sizeof(_member_tree_node))))
    return NULL;
  (*current)->key = strdup(key);
  return &(*current)->value;
};

void _json_member_tree_free(_member_tree_node *node) {
  if (!node) {
    return;
  }
  _json_member_tree_free(node->left);
  _json_member_tree_free(node->right);
  free((void *)node->key);
  json_free_element(node->value);
  free(node->value);
  free(node);
}

int _json_member_tree_delete(_member_tree_node **root, const char *key) {
  _member_tree_node **current = root;

  while (*current && strcmp((*current)->key, key) != 0) {
    current = (strcmp((*current)->key, key) > 0) ? &(*current)->left
                                                 : &(*current)->right;
  }

  if (!*current)
    return -1;

  _member_tree_node *to_delete = *current;

  if (!to_delete->left || !to_delete->right) {
    *current = to_delete->left ? to_delete->left : to_delete->right;
    json_free_element(to_delete->value);
    free((void *)to_delete->key);
    free(to_delete);
  } else {
    _member_tree_node **successor_ptr = &to_delete->right;
    while ((*successor_ptr)->left)
      successor_ptr = &(*successor_ptr)->left;

    _member_tree_node *s = *successor_ptr;

    //
    free((void *)to_delete->key);
    json_free_element(to_delete->value);
    to_delete->key = s->key;
    to_delete->value = s->value;

    *successor_ptr = s->right;
    free(s);
  }
  return 0;
}
 */
