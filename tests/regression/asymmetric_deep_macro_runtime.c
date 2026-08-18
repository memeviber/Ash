#define BASALT_M0(x) ((x) + 1)
#define BASALT_M1(x) (BASALT_M0(x) * 2)
#define BASALT_M2(x) (BASALT_M1(x) + BASALT_M0(x))
#define BASALT_M3(x) (BASALT_M2(x) * BASALT_M1(x))
#define BASALT_M4(x) (BASALT_M3(x) + BASALT_M2(x))
#define BASALT_M5(x) (BASALT_M4(x) * BASALT_M3(x))

int deep_macro(int x) {
    return BASALT_M5(x);
}
