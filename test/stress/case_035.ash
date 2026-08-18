func main(): int {
  let a: int = ((((0 - 1) / 2) * ((0 - 9) + (0 - 1))) & 13);
  let b: int = ((1 ^ 6) ^ 28);
  let c: int = (a + b);
  if (c & 1) == 0 then {
    print c;
  } else {
    print (c ^ 7);
  }
  return 0;
}
