func main(): int {
  let a: int = (((6 | 1) & 5) | 26);
  let b: int = (((0 - 3) / 2) ^ 1);
  let c: int = (a + b);
  if (c & 1) == 0 then {
    print c;
  } else {
    print (c ^ 7);
  }
  return 0;
}
