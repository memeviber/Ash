#include <stdint.h>


int ffi_matrix_abs(int value) {
  return value < 0 ? -value : value;
}

uint64_t ffi_matrix_u64(uint64_t value) {
  return value + UINT64_C(1);
}

double ffi_matrix_double(double value) {
  return value * 2.0;
}

char *ffi_matrix_echo(const char *value) {
  return (char *)value;
}

int ffi_matrix_read(int *value) {
  return value == NULL ? -1 : *value;
}

void ffi_matrix_touch(int *value) {
  if (value != NULL) {
    *value += 1;
  }
}
