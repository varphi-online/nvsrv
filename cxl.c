#include "cxl.h"
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
