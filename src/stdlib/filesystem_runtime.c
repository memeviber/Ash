#ifndef BASALT_FILESYSTEM_RUNTIME_C
#define BASALT_FILESYSTEM_RUNTIME_C

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
#include <io.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

static int filesystem_last_status = 0;

typedef struct filesystem_file {
  FILE *stream;
} filesystem_file;

static int filesystem_error_code(int value) {
  if (value == EINVAL) return 1;
  if (value == ENOENT || value == ENOTDIR) return 2;
  if (value == EACCES || value == EPERM) return 3;
  return 4;
}

static char *filesystem_copy(const char *value, size_t length) {
  char *result;
  if (length > (size_t)-1 - 1) basalt_panic(1);
  result = (char *)malloc(length + 1);
  if (!result) basalt_panic(5);
  if (length > 0) memcpy(result, value, length);
  result[length] = '\0';
  return (char *)basalt_track(result);
}

static char *filesystem_empty(void) {
  return filesystem_copy("", 0);
}

int filesystem_error(void) {
  return filesystem_last_status;
}

void *filesystem_open(const char *path, const char *mode) {
  FILE *file;
  filesystem_file *result;
  if (!path || !mode) {
    filesystem_last_status = 1;
    return NULL;
  }
  file = fopen(path, mode);
  if (!file) {
    filesystem_last_status = filesystem_error_code(errno);
    return NULL;
  }
  result = (filesystem_file *)malloc(sizeof(*result));
  if (!result) {
    fclose(file);
    basalt_panic(5);
  }
  result->stream = file;
  filesystem_last_status = 0;
  return basalt_track(result);
}

char *filesystem_read(void *handle, int max_bytes) {
  filesystem_file *value = (filesystem_file *)handle;
  FILE *file;
  char *buffer;
  size_t requested;
  size_t count;
  if (!value || basalt_find(value) == (size_t)-1 || max_bytes < 0 || max_bytes > 16777216) {
    filesystem_last_status = 1;
    return filesystem_empty();
  }
  file = value->stream;
  requested = (size_t)max_bytes;
  buffer = (char *)malloc(requested + 1);
  if (!buffer) basalt_panic(5);
  count = fread(buffer, 1, requested, file);
  if (ferror(file)) {
    filesystem_last_status = filesystem_error_code(errno);
    free(buffer);
    return filesystem_empty();
  }
  buffer[count] = '\0';
  filesystem_last_status = 0;
  return (char *)basalt_track(buffer);
}

int filesystem_write(void *handle, const char *data) {
  filesystem_file *value = (filesystem_file *)handle;
  FILE *file;
  size_t length;
  size_t count;
  if (!value || basalt_find(value) == (size_t)-1 || !data) {
    filesystem_last_status = 1;
    return -1;
  }
  file = value->stream;
  length = strlen(data);
  count = fwrite(data, 1, length, file);
  if (count != length || fflush(file) != 0) {
    filesystem_last_status = filesystem_error_code(errno);
    if (count > (size_t)2147483647) return 2147483647;
    return (int)count;
  }
  if (count > (size_t)2147483647) {
    filesystem_last_status = 1;
    return 2147483647;
  }
  filesystem_last_status = 0;
  return (int)count;
}

int filesystem_close(void *handle) {
  filesystem_file *value = (filesystem_file *)handle;
  uintptr_t address;
  int status;
  if (!value || basalt_find(value) == (size_t)-1) {
    filesystem_last_status = 1;
    return 1;
  }
  address = (uintptr_t)value;
  status = fclose(value->stream);
  basalt_release((void *)address);
  if (status != 0) {
    filesystem_last_status = filesystem_error_code(errno);
    return filesystem_last_status;
  }
  filesystem_last_status = 0;
  return 0;
}

int filesystem_metadata(const char *path, int64_t *size, int64_t *modified, int *kind) {
#if defined(_WIN32)
  struct _stat64 info;
#else
  struct stat info;
#endif
  if (!path || !size || !modified || !kind) {
    filesystem_last_status = 1;
    return 1;
  }
#if defined(_WIN32)
  if (_stat64(path, &info) != 0) {
#else
  if (stat(path, &info) != 0) {
#endif
    filesystem_last_status = filesystem_error_code(errno);
    return filesystem_last_status;
  }
  *size = (int64_t)info.st_size;
  *modified = (int64_t)info.st_mtime;
#if defined(_WIN32)
  if ((info.st_mode & _S_IFMT) == _S_IFREG) *kind = 1;
  else if ((info.st_mode & _S_IFMT) == _S_IFDIR) *kind = 2;
  else *kind = 3;
#else
  if (S_ISREG(info.st_mode)) *kind = 1;
  else if (S_ISDIR(info.st_mode)) *kind = 2;
  else *kind = 3;
#endif
  filesystem_last_status = 0;
  return 0;
}

#if defined(_WIN32)
typedef struct filesystem_dir {
  intptr_t handle;
  int first;
  struct _finddata64_t entry;
} filesystem_dir;
#else
typedef struct filesystem_dir {
  DIR *handle;
} filesystem_dir;
#endif

void *filesystem_dir_open(const char *path) {
  filesystem_dir *result;
  if (!path) {
    filesystem_last_status = 1;
    return NULL;
  }
  result = (filesystem_dir *)malloc(sizeof(*result));
  if (!result) basalt_panic(5);
#if defined(_WIN32)
  {
    size_t length = strlen(path);
    char *pattern = (char *)malloc(length + 3);
    intptr_t handle;
    if (!pattern) basalt_panic(5);
    memcpy(pattern, path, length);
    if (length == 0 || (path[length - 1] != '\\' && path[length - 1] != '/')) {
      pattern[length++] = '\\';
    }
    pattern[length++] = '*';
    pattern[length] = '\0';
    handle = _findfirst64(pattern, &result->entry);
    free(pattern);
    if (handle == (intptr_t)-1) {
      filesystem_last_status = filesystem_error_code(errno);
      free(result);
      return NULL;
    }
    result->handle = handle;
    result->first = 1;
  }
#else
  result->handle = opendir(path);
  if (!result->handle) {
    filesystem_last_status = filesystem_error_code(errno);
    free(result);
    return NULL;
  }
#endif
  filesystem_last_status = 0;
  return basalt_track(result);
}

char *filesystem_dir_next(void *directory) {
  filesystem_dir *value = (filesystem_dir *)directory;
  if (!value || basalt_find(value) == (size_t)-1) {
    filesystem_last_status = 1;
    return filesystem_empty();
  }
#if defined(_WIN32)
  for (;;) {
    int status;
    if (value->first) {
      value->first = 0;
      status = 0;
    } else {
      status = _findnext64(value->handle, &value->entry);
    }
    if (status != 0) {
      filesystem_last_status = 5;
      return filesystem_empty();
    }
    if (strcmp(value->entry.name, ".") == 0 || strcmp(value->entry.name, "..") == 0) continue;
    filesystem_last_status = 0;
    return filesystem_copy(value->entry.name, strlen(value->entry.name));
  }
#else
  errno = 0;
  for (;;) {
    struct dirent *entry = readdir(value->handle);
    if (!entry) {
      if (errno == 0) filesystem_last_status = 5;
      else filesystem_last_status = filesystem_error_code(errno);
      return filesystem_empty();
    }
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
    filesystem_last_status = 0;
    return filesystem_copy(entry->d_name, strlen(entry->d_name));
  }
#endif
}

int filesystem_dir_close(void *directory) {
  filesystem_dir *value = (filesystem_dir *)directory;
  int status = 0;
  if (!value || basalt_find(value) == (size_t)-1) {
    filesystem_last_status = 1;
    return 1;
  }
#if defined(_WIN32)
  status = _findclose(value->handle);
#else
  status = closedir(value->handle);
#endif
  if (status != 0) {
    filesystem_last_status = filesystem_error_code(errno);
    basalt_release(value);
    return filesystem_last_status;
  }
  basalt_release(value);
  filesystem_last_status = 0;
  return 0;
}

#endif
