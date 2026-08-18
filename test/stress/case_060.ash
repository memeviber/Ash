func main(): int {
  let a: int = (((6 + 1) + (6 | 6)) & 5);
  let b: int = ((7 | 28) ^ 7);
  let c: int = (a + b);
  if (c & 1) == 0 then {
    print c;
  } else {
    print (c ^ 7);
  }
  return 0;
}
