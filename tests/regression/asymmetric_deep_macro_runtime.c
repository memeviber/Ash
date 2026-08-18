#define PYREL_M0(x) ((x) + 1)
#define PYREL_M1(x) (PYREL_M0(x) * 2)
#define PYREL_M2(x) (PYREL_M1(x) + PYREL_M0(x))
#define PYREL_M3(x) (PYREL_M2(x) * PYREL_M1(x))
#define PYREL_M4(x) (PYREL_M3(x) + PYREL_M2(x))
#define PYREL_M5(x) (PYREL_M4(x) * PYREL_M3(x))

int deep_macro(int x) {
    return PYREL_M5(x);
}
