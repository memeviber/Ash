func main(): int {
  let a: int = (((5 ^ 30) | 19) & 0);
  let b: int = (30 << 0);
  let c: int = (a + b);
  if (c & 1) == 0 then {
    print c;
  } else {
    print (c ^ 7);
  }
  return 0;
}
