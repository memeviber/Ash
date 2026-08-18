func main(): int {
  let a: int = (((11 * 9) & 59) - ((30 * (0 - 25)) & 47));
  let b: int = ((1 + 8) + ((0 - 23) / 7));
  let c: int = (a + b);
  if c != 0 then {
    print c;
  } else {
    print 0;
  }
  return 0;
}
