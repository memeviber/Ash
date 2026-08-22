#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static int parse_nonnegative(const char *value, int *result) {
  char *end = NULL;
  long parsed;
  if (!value || !result || value[0] == '\0')
    return 0;
  errno = 0;
  parsed = strtol(value, &end, 10);
  if (errno != 0 || end == value || *end != '\0' || parsed < 0 || parsed > 1000000)
    return 0;
  *result = (int)parsed;
  return 1;
}

int main(int argc, char **argv) {
  int count;
  int i;
  if (argc < 2)
    return 64;
  if (strcmp(argv[1], "args") == 0) {
    for (i = 2; i < argc; i++)
      (void)printf("%d=%s\n", i - 2, argv[i]);
    return 0;
  }
  if (strcmp(argv[1], "streams") == 0) {
    if (argc != 3 || !parse_nonnegative(argv[2], &count))
      return 65;
    (void)setvbuf(stdout, NULL, _IONBF, 0);
    (void)setvbuf(stderr, NULL, _IONBF, 0);
    for (i = 0; i < count; i++) {
      (void)fputc('O', stdout);
      (void)fputc('E', stderr);
    }
    return 0;
  }
  if (strcmp(argv[1], "exit") == 0) {
    if (argc != 3 || !parse_nonnegative(argv[2], &count) || count > 255)
      return 66;
    return count;
  }
  return 67;
}
