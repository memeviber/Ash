#include <stdint.h>
#define PYREL_SCALE(x) ((x) * 3)
#define PYREL_BIAS(x, y) ((x) + (y) * 2)
int macro_scale(int x) {
    return PYREL_SCALE(x);
}
int macro_bias(int x, int y) {
    return PYREL_BIAS(x, y);
}
