#include <stdlib.h>
#include <string.h>

int c_abs(int x) {
  return x < 0 ? -x : x;
}

int c_strlen(const char *s) {
  return (int)strlen(s);
}
