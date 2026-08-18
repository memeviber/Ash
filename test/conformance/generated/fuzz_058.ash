func main(): int {
  let a: int = (((11 + 0) ^ 43) << 1);
  let b: int = (((0 - 27) | 9) ^ 15);
  let c: int = (a + b);
  if c != 0 then {
    print c;
  } else {
    print 0;
  }
  return 0;
}
