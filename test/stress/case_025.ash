func main(): int {
  let a: int = (((38 << 1) / 1) - 3);
  let b: int = (1 >> 1);
  let c: int = (a + b);
  if (c & 1) == 0 then {
    print c;
  } else {
    print (c ^ 7);
  }
  return 0;
}
