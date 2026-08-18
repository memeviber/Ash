func main(): int {
  let a: int = ((((0 - 9) & 8) + 0) | 7);
  let b: int = ((5 / 5) + 2);
  let c: int = (a + b);
  if (c & 1) == 0 then {
    print c;
  } else {
    print (c ^ 7);
  }
  return 0;
}
