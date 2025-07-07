#include "json.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

typedef struct _json_kv_entry {
  const char *key;
  json_element *value;
  struct _json_kv_entry *next;
} _json_kv_entry;

// Frees heap allocated kv entry and its children
void _json_kv_free(_json_kv_entry *kv) {
  free((void *)kv->key);
  json_free_element(kv->value);
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
    free(element);
    break;
    // TODO: all other types
  }
}

json_element **_json_kv_insert(_json_kv_entry **root, char *key) {
  if (*root == NULL) {
    *root = calloc(1, sizeof(_json_kv_entry));
    (*root)->key = strdup(key);
    return &(*root)->value;
  }
  _json_kv_entry *current = *root;
  while (current->next) {
    current = current->next;
  }
  current->next = calloc(1, sizeof(_json_kv_entry));
  current->next->key = strdup(key);
  return &current->next->value;
}

json_element *_json_kv_search(_json_kv_entry *root, char *key) {
  _json_kv_entry *current = root;
  while (current) {
    if (strcmp(current->key, key) == 0)
      return current->value;
    current = current->next;
  }
  return NULL;
}

void _json_kv_delete(_json_kv_entry **root, char *key) {
  _json_kv_entry *current = *root;
  _json_kv_entry *prev = NULL;

  while (current) {
    if (strcmp(current->key, key) == 0) {
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
    if (strcmp(current->key, key) == 0) {
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
