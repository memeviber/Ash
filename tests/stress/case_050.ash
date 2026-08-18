func main(): int {
  let a: int = (((4 - 3) / 3) & 1);
  let b: int = (18 >> 0);
  let c: int = (a + b);
  if (c & 1) == 0 then {
    print c;
  } else {
    print (c ^ 7);
  }
  return 0;
}
