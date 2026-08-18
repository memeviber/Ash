#include <stdint.h>
#define ASH_SCALE(x) ((x) * 3)
#define ASH_BIAS(x, y) ((x) + (y) * 2)
int macro_scale(int x) {
    return ASH_SCALE(x);
}
int macro_bias(int x, int y) {
    return ASH_BIAS(x, y);
}
