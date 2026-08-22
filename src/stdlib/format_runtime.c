#ifndef BASALT_FORMAT_RUNTIME_C
#define BASALT_FORMAT_RUNTIME_C

#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *basalt_format_copy(const char *format, ...) {
  va_list arguments;
  va_list sizing;
  int length;
  char *result;
  va_start(arguments, format);
  va_copy(sizing, arguments);
  length = vsnprintf(NULL, 0, format, sizing);
  va_end(sizing);
  if (length < 0) {
    va_end(arguments);
    basalt_panic(1);
  }
  result = (char *)malloc((size_t)length + 1);
  if (!result) {
    va_end(arguments);
    basalt_panic(5);
  }
  (void)vsnprintf(result, (size_t)length + 1, format, arguments);
  va_end(arguments);
  return (char *)basalt_track(result);
}

char *basalt_format_int(int value) {
  return basalt_format_copy("%d", value);
}

char *basalt_format_i64(int64_t value) {
  return basalt_format_copy("%lld", (long long)value);
}

char *basalt_format_f64(double value) {
  return basalt_format_copy("%g", value);
}

char *basalt_format_char(int value) {
  return basalt_format_copy("%c", value);
}

#endif
