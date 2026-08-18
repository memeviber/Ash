#define ASH_M0(x) ((x) + 1)
#define ASH_M1(x) (ASH_M0(x) * 2)
#define ASH_M2(x) (ASH_M1(x) + ASH_M0(x))
#define ASH_M3(x) (ASH_M2(x) * ASH_M1(x))
#define ASH_M4(x) (ASH_M3(x) + ASH_M2(x))
#define ASH_M5(x) (ASH_M4(x) * ASH_M3(x))

int deep_macro(int x) {
    return ASH_M5(x);
}
