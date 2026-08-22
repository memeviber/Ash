#ifndef BASALT_STRING_RUNTIME_INCLUDED
#define BASALT_STRING_RUNTIME_INCLUDED 1

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

void str_free_owned(const char *value) {
  char *owned;
  owned = (char *)value;
  basalt_release(owned);
}

char *str_alloc_owned(int count) {
  char *result;
  if (count < 1) count = 1;
  result = (char *)calloc((size_t)count, sizeof(char));
  if (!result) basalt_panic(5);
  return (char *)basalt_track(result);
}
char *str_copy_owned(char *data, int count) {
  char *result;
  int i;
  if (count < 0) count = 0;
  result = str_alloc_owned(count + 1);
  for (i = 0; i < count; i++) result[i] = data[i];
  result[count] = '\0';
  return result;
}
#endif
