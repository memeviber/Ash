#ifndef BASALT_STDLIB_UTF8_RUNTIME_H
#define BASALT_STDLIB_UTF8_RUNTIME_H

static char basalt_utf8_bad_continuation[] = "\x80";
static char basalt_utf8_bad_overlong_two[] = "\xC0\x80";
static char basalt_utf8_bad_surrogate[] = "\xED\xA0\x80";
static char basalt_utf8_bad_too_high[] = "\xF4\x90\x80\x80";
static char basalt_utf8_bad_truncated[] = "\xE2\x82";

char *utf8_fixture_bad_continuation(void) {
  return basalt_utf8_bad_continuation;
}

char *utf8_fixture_bad_overlong_two(void) {
  return basalt_utf8_bad_overlong_two;
}

char *utf8_fixture_bad_surrogate(void) {
  return basalt_utf8_bad_surrogate;
}

char *utf8_fixture_bad_too_high(void) {
  return basalt_utf8_bad_too_high;
}

char *utf8_fixture_bad_truncated(void) {
  return basalt_utf8_bad_truncated;
}

#endif
