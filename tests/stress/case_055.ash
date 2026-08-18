func main(): int {
  let a: int = (55 >> 2);
  let b: int = (45 << 0);
  let c: int = (a + b);
  if (c & 1) == 0 then {
    print c;
  } else {
    print (c ^ 7);
  }
  return 0;
}
