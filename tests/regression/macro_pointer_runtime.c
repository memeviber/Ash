#include <stdint.h>
#define BASALT_SCALE(x) ((x) * 3)
#define BASALT_BIAS(x, y) ((x) + (y) * 2)
int macro_scale(int x) {
    return BASALT_SCALE(x);
}
int macro_bias(int x, int y) {
    return BASALT_BIAS(x, y);
}
