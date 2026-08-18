func main(): int {
  let a: int = (((3 + 8) ^ 14) + 2);
  let b: int = (((0 - 8) - 2) - 1);
  let c: int = (a + b);
  if (c & 1) == 0 then {
    print c;
  } else {
    print (c ^ 7);
  }
  return 0;
}
