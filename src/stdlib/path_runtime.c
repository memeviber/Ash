#ifndef BASALT_PATH_RUNTIME_INCLUDED
#define BASALT_PATH_RUNTIME_INCLUDED 1

static char basalt_path_separator_char(void) {
#if defined(_WIN32)
  return '\\';
#else
  return '/';
#endif
}
#endif
