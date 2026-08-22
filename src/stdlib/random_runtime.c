#ifndef BASALT_RANDOM_RUNTIME_C
#define BASALT_RANDOM_RUNTIME_C

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(_WIN32)
#include <errno.h>
#endif

typedef struct basalt_rng {
  uint64_t state;
} basalt_rng;

static int random_last_status = 0;

static uint64_t random_splitmix64(uint64_t *state) {
  uint64_t value = *state + UINT64_C(0x9e3779b97f4a7c15);
  value = (value ^ (value >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
  value = (value ^ (value >> 27)) * UINT64_C(0x94d049bb133111eb);
  return value ^ (value >> 31);
}

int basalt_random_status(void) {
  return random_last_status;
}

void *basalt_random_seed(uint64_t seed) {
  basalt_rng *rng = (basalt_rng *)malloc(sizeof(*rng));
  if (!rng) {
    random_last_status = 5;
    return NULL;
  }
  rng->state = seed;
  random_last_status = 0;
  return basalt_track(rng);
}

uint64_t basalt_random_next_u64(void *value) {
  basalt_rng *rng = (basalt_rng *)value;
  if (!rng || basalt_find(rng) == (size_t)-1) {
    random_last_status = 1;
    return 0;
  }
  random_last_status = 0;
  return random_splitmix64(&rng->state);
}

uint32_t basalt_random_next_u32(void *value) {
  return (uint32_t)(basalt_random_next_u64(value) >> 32);
}

uint64_t basalt_random_next_bounded(void *value, uint64_t bound) {
  uint64_t threshold;
  uint64_t sample;
  if (bound == 0) {
    random_last_status = 1;
    return 0;
  }
  threshold = (uint64_t)(0 - bound) % bound;
  do {
    sample = basalt_random_next_u64(value);
    if (random_last_status != 0) return 0;
  } while (sample < threshold);
  random_last_status = 0;
  return sample % bound;
}

double basalt_random_next_float(void *value) {
  uint64_t sample = basalt_random_next_u64(value);
  if (random_last_status != 0) return 0.0;
  return (double)(sample >> 11) * (1.0 / 9007199254740992.0);
}

int basalt_random_entropy(uint64_t *output) {
  if (!output) {
    random_last_status = 1;
    return 1;
  }
#if defined(_WIN32)
  {
    unsigned int first;
    unsigned int second;
    if (rand_s(&first) != 0 || rand_s(&second) != 0) {
      random_last_status = 2;
      return 2;
    }
    *output = ((uint64_t)first << 32) | (uint64_t)second;
  }
#else
  {
    FILE *file = fopen("/dev/urandom", "rb");
    if (!file) {
      random_last_status = 2;
      return 2;
    }
    if (fread(output, sizeof(*output), 1, file) != 1) {
      fclose(file);
      random_last_status = 2;
      return 2;
    }
    if (fclose(file) != 0) {
      random_last_status = 2;
      return 2;
    }
  }
#endif
  random_last_status = 0;
  return 0;
}

void basalt_random_free(void *value) {
  if (!value) return;
  basalt_release(value);
}

#endif
