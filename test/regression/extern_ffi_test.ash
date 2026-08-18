includec "extern_ffi_test.c"

extern func c_abs(x: int): int;
extern func c_strlen(s: string): int;

func main(): int {
  let a: int = c_abs(0 - 7);
  let n: int = c_strlen("ash");
  print a;
  print n;
  return 0;
}

