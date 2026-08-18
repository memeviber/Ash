func main(): int {
  let a: int = ((((0 - 8) - (0 - 12)) - (7 + 0)) + 2);
  let b: int = (((0 - 4) + 3) & 3);
  let c: int = (a + b);
  if (c & 1) == 0 then {
    print c;
  } else {
    print (c ^ 7);
  }
  return 0;
}
