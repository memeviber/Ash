func main(): int {
  let a: int = ((1 >> 1) & 2);
  let b: int = ((10 | 24) & 7);
  let c: int = (a + b);
  if (c & 1) == 0 then {
    print c;
  } else {
    print (c ^ 7);
  }
  return 0;
}
