#ifndef BASALT_TIME_RUNTIME_C
#define BASALT_TIME_RUNTIME_C

#include <stdint.h>
#include <time.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <errno.h>
#include <unistd.h>
#endif

static int time_last_status = 0;

int basalt_time_status(void) {
  return time_last_status;
}

int64_t basalt_time_monotonic_ns(void) {
#if defined(_WIN32)
  static LARGE_INTEGER frequency;
  LARGE_INTEGER counter;
  if (frequency.QuadPart == 0 && !QueryPerformanceFrequency(&frequency)) {
    time_last_status = 1;
    return -1;
  }
  if (!QueryPerformanceCounter(&counter)) {
    time_last_status = 1;
    return -1;
  }
  time_last_status = 0;
  return (int64_t)((counter.QuadPart * 1000000000LL) / frequency.QuadPart);
#else
  struct timespec value;
  if (clock_gettime(CLOCK_MONOTONIC, &value) != 0) {
    time_last_status = 1;
    return -1;
  }
  time_last_status = 0;
  return (int64_t)value.tv_sec * 1000000000LL + (int64_t)value.tv_nsec;
#endif
}

int64_t basalt_time_wall_seconds(void) {
  time_t value = time(NULL);
  if (value == (time_t)-1) {
    time_last_status = 1;
    return -1;
  }
  time_last_status = 0;
  return (int64_t)value;
}

int basalt_time_sleep_ms(int64_t milliseconds) {
  if (milliseconds < 0 || milliseconds > 2147483647LL) {
    time_last_status = 1;
    return 1;
  }
#if defined(_WIN32)
  Sleep((DWORD)milliseconds);
  time_last_status = 0;
  return 0;
#else
  {
    struct timespec request;
    request.tv_sec = (time_t)(milliseconds / 1000LL);
    request.tv_nsec = (long)((milliseconds % 1000LL) * 1000000LL);
    while (nanosleep(&request, &request) != 0) {
      if (errno != EINTR) {
        time_last_status = 1;
        return 1;
      }
    }
  }
  time_last_status = 0;
  return 0;
#endif
}

#endif
