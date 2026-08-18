func main(): int {
  let a: int = ((((0 - 5) | 11) + (33 << 1)) - 1);
  let b: int = ((8 & 16) + (11 - 2));
  let c: int = (a + b);
  if (c & 1) == 0 then {
    print c;
  } else {
    print (c ^ 7);
  }
  return 0;
}
