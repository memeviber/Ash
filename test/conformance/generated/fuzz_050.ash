func main(): int {
  let a: int = (((11 & 7) ^ 19) ^ 42);
  let b: int = (((0 - 20) - 18) ^ 63);
  let c: int = (a + b);
  if c != 0 then {
    print c;
  } else {
    print 0;
  }
  return 0;
}
