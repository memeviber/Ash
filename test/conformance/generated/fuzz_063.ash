func main(): int {
  let a: int = ((((0 - 15) + 8) ^ 10) / 7);
  let b: int = ((14 & 43) * (8 ^ 1));
  let c: int = (a + b);
  if c != 0 then {
    print c;
  } else {
    print 0;
  }
  return 0;
}
