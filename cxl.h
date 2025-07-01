#ifndef CXL_H
#define CXL_H

#include <stddef.h>

char *cxl_join_with_delim(char *dest, char **strings, size_t number,
                          char *delimiter);

#endif
