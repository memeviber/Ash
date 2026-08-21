#include <stdio.h>
#include <stdlib.h>

#define ROW_COUNT 20000
#define COLUMN_COUNT 32
#define RECURSION_DEPTH 128
#define MODULUS 1000003

typedef struct {
  int *data;
  int len;
  int cap;
} Array;

typedef struct {
  Array cells;
  Array row_seeds;
  int rows;
  int columns;
} Matrix;

typedef struct {
  Matrix primary;
  Matrix secondary;
  int rounds;
} Workspace;

static Array array_new(int capacity) {
  Array a = {0};
  int cap = capacity;
  if (cap < 1) {
    cap = 4;
  }
  a.data = (int *)calloc((size_t)cap, sizeof(int));
  if (a.data == NULL) {
    return a;
  }
  a.cap = cap;
  return a;
}

static Array array_reserve(Array a, int minimum) {
  if (minimum < 1 || minimum <= a.cap) {
    return a;
  }
  int *grown = (int *)realloc(a.data, (size_t)minimum * sizeof(int));
  if (grown == NULL) {
    free(a.data);
    a.data = NULL;
    a.len = 0;
    a.cap = 0;
    return a;
  }
  int old_cap = a.cap;
  a.data = grown;
  a.cap = minimum;
  int i = old_cap;
  while (i < minimum) {
    a.data[i] = 0;
    i = i + 1;
  }
  return a;
}

static Array array_ensure_capacity(Array a, int required) {
  if (required < 1 || required <= a.cap) {
    return a;
  }
  int next = a.cap;
  if (next < 1) {
    next = 4;
  }
  while (next < required) {
    int grown = next * 2;
    if (grown < next) {
      grown = required;
    }
    next = grown;
  }
  return array_reserve(a, next);
}

static Array array_push(Array a, int value) {
  a = array_ensure_capacity(a, a.len + 1);
  if (a.data != NULL) {
    a.data[a.len] = value;
    a.len = a.len + 1;
  }
  return a;
}

static int array_get(Array a, int index) {
  return a.data[index];
}

static Array array_free(Array a) {
  free(a.data);
  a.data = NULL;
  a.len = 0;
  a.cap = 0;
  return a;
}

static int deep_mix(int value, int depth) {
  if (depth == 0) {
    return value;
  }
  int mixed = (value * 33 + depth) - ((value * 33 + depth) / MODULUS) * MODULUS;
  return deep_mix(mixed, depth - 1);
}

static Matrix make_matrix(int rows, int columns, int salt) {
  Matrix matrix = {0};
  matrix.cells = array_new(rows * columns);
  matrix.row_seeds = array_new(rows);
  matrix.rows = rows;
  matrix.columns = columns;
  int row = 0;
  while (row < rows) {
    matrix.row_seeds = array_push(matrix.row_seeds, row * 17 + salt);
    int column = 0;
    while (column < columns) {
      int index = row * columns + column;
      int value = (index * 13 + salt) - ((index * 13 + salt) / 997) * 997;
      matrix.cells = array_push(matrix.cells, value);
      column = column + 1;
    }
    row = row + 1;
  }
  return matrix;
}

static int checksum_matrix(Matrix matrix, int depth) {
  int checksum = 0;
  int row = 0;
  while (row < matrix.rows) {
    int row_seed = array_get(matrix.row_seeds, row);
    int mixed_seed = deep_mix(row_seed, depth);
    checksum = (checksum + mixed_seed) - ((checksum + mixed_seed) / MODULUS) * MODULUS;
    int column = 0;
    while (column < matrix.columns) {
      int index = row * matrix.columns + column;
      int value = array_get(matrix.cells, index);
      int contribution = value * 3 + row + column;
      checksum = (checksum + contribution) - ((checksum + contribution) / MODULUS) * MODULUS;
      column = column + 1;
    }
    row = row + 1;
  }
  return checksum;
}

static void free_matrix(Matrix matrix) {
  (void)array_free(matrix.cells);
  (void)array_free(matrix.row_seeds);
}

int main(void) {
  Workspace workspace = {0};
  workspace.primary = make_matrix(ROW_COUNT, COLUMN_COUNT, 11);
  workspace.secondary = make_matrix(ROW_COUNT, COLUMN_COUNT, 29);
  if (workspace.primary.cells.data == NULL || workspace.secondary.cells.data == NULL) {
    free_matrix(workspace.primary);
    free_matrix(workspace.secondary);
    return 1;
  }
  workspace.rounds = RECURSION_DEPTH;
  int left = checksum_matrix(workspace.primary, workspace.rounds);
  int right = checksum_matrix(workspace.secondary, workspace.rounds);
  printf("%d\n", left);
  printf("%d\n", right);
  free_matrix(workspace.primary);
  free_matrix(workspace.secondary);
  return 0;
}
