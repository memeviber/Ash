func main(): int {
  let a: int = (53 >> 1);
  let b: int = (((0 - 8) & 16) - 2);
  let c: int = (a + b);
  if (c & 1) == 0 then {
    print c;
  } else {
    print (c ^ 7);
  }
  return 0;
}
