#ifndef CXL_H
#define CXL_H

#include <stddef.h>

char *cxl_join_with_delim(char *dest, char **strings, size_t number,
                          char *delimiter);

// Destructively strips a string's whitespace in place, result
// is passed pointer
void strip_whitespace_i(char *str);

// Returns a new string with whitespace stripped
char *strip_whitespace(char *str);

typedef struct StringBuilder {
} StringBuilder;
#endif
