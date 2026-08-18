func main(): int {
  let a: int = (60 << 0);
  let b: int = (17 >> 2);
  let c: int = (a + b);
  if (c & 1) == 0 then {
    print c;
  } else {
    print (c ^ 7);
  }
  return 0;
}
