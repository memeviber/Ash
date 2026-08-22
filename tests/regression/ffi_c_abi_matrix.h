#ifndef BASALT_FFI_C_ABI_MATRIX_H
#define BASALT_FFI_C_ABI_MATRIX_H

#include <stdint.h>
#include <stddef.h>


int ffi_matrix_abs(int value);
uint64_t ffi_matrix_u64(uint64_t value);
double ffi_matrix_double(double value);
char *ffi_matrix_echo(const char *value);
int ffi_matrix_read(int *value);
void ffi_matrix_touch(int *value);

#endif
