func main(): int {
  let a: int = ((((0 - 7) + 2) | 15) | 8);
  let b: int = (24 << 2);
  let c: int = (a + b);
  if (c & 1) == 0 then {
    print c;
  } else {
    print (c ^ 7);
  }
  return 0;
}
