#include "cxl.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/*
 * Joins an array of strings by some delimiter into a pre-allocated destination
 *
 * Sourced from:
 * https://stackoverflow.com/questions/25767841/how-do-i-concatenate-the-string-elements-of-my-array-into-a-single-string-in-c
 */
char *cxl_join_with_delim(char *dest, char **strings, size_t number,
                          char *delimiter) {
  size_t glue_length = strlen(delimiter);

  char *target = dest; // where to copy the next elements
  *target = '\0';
  for (size_t i = 0; i < number; i++) {
    if (i > 0) { // need delimiter
      strcat(target, delimiter);
      target += glue_length;
    }
    strcat(target, strings[i]);
    target += strlen(strings[i]); // move to the end
  };
  return dest;
}

void strip_whitespace_i(char *str) {
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

char *http_strip_whitespace(char *str) {
  if (str == NULL || *str == '\0') {
    return 0;
  }
  char *start = str;
  while (isspace((unsigned char)*start)) {
    start++;
  }
  if (*start == '\0') {
    *str = '\0';
    return 0;
  }
  char *end = start + strlen(start) - 1;
  while (end > start && isspace((unsigned char)*end)) {
    end--;
  }
  *(end + 1) = '\0';

  char *out = malloc((end - start + 2) * sizeof(char));
  memcpy(out, start, end - start + 2);
  return out;
}
