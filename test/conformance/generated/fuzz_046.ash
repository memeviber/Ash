func main(): int {
  let a: int = (((2 - 21) - (10 & 8)) & 14);
  let b: int = ((29 + 12) >> 2);
  let c: int = (a + b);
  if c != 0 then {
    print c;
  } else {
    print 0;
  }
  return 0;
}
