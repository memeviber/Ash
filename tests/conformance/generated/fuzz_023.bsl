func main(): int {
  let a: int = (((12 >> 1) ^ 11) / 3);
  let b: int = (((0 - 6) * (0 - 31)) - (27 + (0 - 4)));
  let c: int = (a + b);
  if c != 0 then {
    print c;
  } else {
    print 0;
  }
  return 0;
}
