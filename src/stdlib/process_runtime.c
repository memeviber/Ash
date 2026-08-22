#ifndef BASALT_PROCESS_RUNTIME_C
#define BASALT_PROCESS_RUNTIME_C

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
#include <direct.h>
#include <process.h>
#else
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

static int process_last_status = 0;

static int process_error_code(int value) {
  if (value == EINVAL) return 1;
  if (value == ENOENT || value == ENOTDIR) return 2;
  if (value == EACCES || value == EPERM) return 3;
  return 4;
}

static char *process_copy(const char *value) {
  size_t length;
  char *result;
  if (!value) value = "";
  length = strlen(value);
  if (length == (size_t)-1) basalt_panic(1);
  result = (char *)malloc(length + 1);
  if (!result) basalt_panic(5);
  memcpy(result, value, length + 1);
  return (char *)basalt_track(result);
}

int basalt_process_status(void) {
  return process_last_status;
}

char *basalt_process_getenv(const char *name) {
  const char *value;
  if (!name || name[0] == '\0' || strchr(name, '=') != NULL) {
    process_last_status = 1;
    return process_copy("");
  }
  value = getenv(name);
  if (!value) {
    process_last_status = 2;
    return process_copy("");
  }
  process_last_status = 0;
  return process_copy(value);
}

int basalt_process_setenv(const char *name, const char *value) {
  if (!name || name[0] == '\0' || strchr(name, '=') != NULL || !value) {
    process_last_status = 1;
    return 1;
  }
#if defined(_WIN32)
  if (_putenv_s(name, value) != 0) {
    process_last_status = process_error_code(errno);
    return process_last_status;
  }
#else
  if (setenv(name, value, 1) != 0) {
    process_last_status = process_error_code(errno);
    return process_last_status;
  }
#endif
  process_last_status = 0;
  return 0;
}

int basalt_process_unsetenv(const char *name) {
  if (!name || name[0] == '\0' || strchr(name, '=') != NULL) {
    process_last_status = 1;
    return 1;
  }
#if defined(_WIN32)
  if (_putenv_s(name, "") != 0) {
    process_last_status = process_error_code(errno);
    return process_last_status;
  }
#else
  if (unsetenv(name) != 0) {
    process_last_status = process_error_code(errno);
    return process_last_status;
  }
#endif
  process_last_status = 0;
  return 0;
}

char *basalt_process_cwd(void) {
  char *result;
#if defined(_WIN32)
  result = _getcwd(NULL, 0);
#else
  result = getcwd(NULL, 0);
#endif
  if (!result) {
    process_last_status = process_error_code(errno);
    return process_copy("");
  }
  {
    char *copy = process_copy(result);
    free(result);
    process_last_status = 0;
    return copy;
  }
}

int basalt_process_chdir(const char *path) {
  if (!path || path[0] == '\0') {
    process_last_status = 1;
    return 1;
  }
#if defined(_WIN32)
  if (_chdir(path) != 0) {
#else
  if (chdir(path) != 0) {
#endif
    process_last_status = process_error_code(errno);
    return process_last_status;
  }
  process_last_status = 0;
  return 0;
}

typedef struct basalt_process_handle {
#if defined(_WIN32)
  intptr_t pid;
#else
  pid_t pid;
#endif
  int waited;
} basalt_process_handle;

void *basalt_process_spawn(const char *executable, char **args, int arg_count) {
  basalt_process_handle *handle;
  if (!executable || executable[0] == '\0' || arg_count < 0 || arg_count > 1048576) {
    process_last_status = 1;
    return NULL;
  }
#if defined(_WIN32)
  {
    char **argv = (char **)calloc((size_t)arg_count + 2, sizeof(*argv));
    intptr_t pid;
    int i;
    if (!argv) basalt_panic(5);
    argv[0] = (char *)executable;
    for (i = 0; i < arg_count; i++) {
      if (!args || !args[i]) {
        free(argv);
        process_last_status = 1;
        return NULL;
      }
      argv[i + 1] = args[i];
    }
    pid = _spawnvp(_P_NOWAIT, executable, (const char *const *)argv);
    free(argv);
    if (pid == (intptr_t)-1) {
      process_last_status = process_error_code(errno);
      return NULL;
    }
    handle = (basalt_process_handle *)calloc(1, sizeof(*handle));
    if (!handle) basalt_panic(5);
    handle->pid = pid;
  }
#else
  {
    char **argv = (char **)calloc((size_t)arg_count + 2, sizeof(*argv));
    int error_pipe[2];
    pid_t pid;
    int i;
    if (!argv) basalt_panic(5);
    argv[0] = (char *)executable;
    for (i = 0; i < arg_count; i++) {
      if (!args || !args[i]) {
        free(argv);
        process_last_status = 1;
        return NULL;
      }
      argv[i + 1] = args[i];
    }
    if (pipe(error_pipe) != 0) {
      free(argv);
      process_last_status = process_error_code(errno);
      return NULL;
    }
    if (fcntl(error_pipe[1], F_SETFD, FD_CLOEXEC) == -1) {
      close(error_pipe[0]);
      close(error_pipe[1]);
      free(argv);
      process_last_status = process_error_code(errno);
      return NULL;
    }
    pid = fork();
    if (pid < 0) {
      close(error_pipe[0]);
      close(error_pipe[1]);
      free(argv);
      process_last_status = process_error_code(errno);
      return NULL;
    }
    if (pid == 0) {
      int exec_error;
      ssize_t written;
      close(error_pipe[0]);
      execvp(executable, argv);
      exec_error = errno;
      written = write(error_pipe[1], &exec_error, sizeof(exec_error));
      (void)written;
      close(error_pipe[1]);
      _exit(127);
    }
    free(argv);
    close(error_pipe[1]);
    {
      int exec_error;
      ssize_t count;
      do {
        count = read(error_pipe[0], &exec_error, sizeof(exec_error));
      } while (count < 0 && errno == EINTR);
      close(error_pipe[0]);
      if (count == (ssize_t)sizeof(exec_error)) {
        (void)waitpid(pid, NULL, 0);
        process_last_status = process_error_code(exec_error);
        return NULL;
      }
    }
    handle = (basalt_process_handle *)calloc(1, sizeof(*handle));
    if (!handle) basalt_panic(5);
    handle->pid = pid;
  }
#endif
  handle->waited = 0;
  process_last_status = 0;
  return basalt_track(handle);
}

static int process_status_value(int status) {
#if defined(_WIN32)
  return status;
#else
  if (WIFEXITED(status)) return WEXITSTATUS(status);
  if (WIFSIGNALED(status)) return 0 - WTERMSIG(status);
  return 1;
#endif
}

int basalt_process_wait(void *value) {
  basalt_process_handle *handle = (basalt_process_handle *)value;
  int status;
  if (!handle || basalt_find(handle) == (size_t)-1 || handle->waited != 0) {
    process_last_status = 1;
    return -1;
  }
#if defined(_WIN32)
  if (_cwait(&status, handle->pid, 0) == -1) {
    process_last_status = process_error_code(errno);
    return -1;
  }
#else
  if (waitpid(handle->pid, &status, 0) < 0) {
    process_last_status = process_error_code(errno);
    return -1;
  }
#endif
  handle->waited = 1;
  status = process_status_value(status);
  basalt_release(handle);
  process_last_status = 0;
  return status;
}

int basalt_process_wait_timeout(void *value, int64_t timeout_ms) {
#if defined(_WIN32)
  (void)value;
  (void)timeout_ms;
  process_last_status = 8;
  return -1;
#else
  basalt_process_handle *handle = (basalt_process_handle *)value;
  int status;
  int64_t elapsed = 0;
  if (!handle || basalt_find(handle) == (size_t)-1 || handle->waited != 0 || timeout_ms < 0) {
    process_last_status = 1;
    return -1;
  }
  for (;;) {
    pid_t result = waitpid(handle->pid, &status, WNOHANG);
    if (result == handle->pid) {
      handle->waited = 1;
      status = process_status_value(status);
      basalt_release(handle);
      process_last_status = 0;
      return status;
    }
    if (result < 0) {
      process_last_status = process_error_code(errno);
      return -1;
    }
    if (elapsed >= timeout_ms) {
      process_last_status = 7;
      return -1;
    }
    {
      struct timespec request;
      request.tv_sec = 0;
      request.tv_nsec = 1000000L;
      nanosleep(&request, NULL);
    }
    elapsed = elapsed + 1;
  }
#endif
}

int basalt_process_signal(void *value, int signal_number) {
#if defined(_WIN32)
  (void)value;
  (void)signal_number;
  process_last_status = 8;
  return 8;
#else
  basalt_process_handle *handle = (basalt_process_handle *)value;
  if (!handle || basalt_find(handle) == (size_t)-1 || signal_number <= 0) {
    process_last_status = 1;
    return 1;
  }
  if (kill(handle->pid, signal_number) != 0) {
    process_last_status = process_error_code(errno);
    return process_last_status;
  }
  process_last_status = 0;
  return 0;
#endif
}

int basalt_process_free(void *value) {
  basalt_process_handle *handle = (basalt_process_handle *)value;
  if (!handle || basalt_find(handle) == (size_t)-1) {
    process_last_status = 1;
    return 1;
  }
  if (handle->waited == 0) {
    process_last_status = 6;
    return 6;
  }
  basalt_release(handle);
  process_last_status = 0;
  return 0;
}

#endif
