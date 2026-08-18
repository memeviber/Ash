func main(): int {
  let a: int = (((12 ^ 18) & 8) & 0);
  let b: int = (((0 - 10) + 3) | 16);
  let c: int = (a + b);
  if (c & 1) == 0 then {
    print c;
  } else {
    print (c ^ 7);
  }
  return 0;
}
