func main(): int {
  let a: int = (((15 << 2) & 5) + 1);
  let b: int = ((10 & 0) & 12);
  let c: int = (a + b);
  if (c & 1) == 0 then {
    print c;
  } else {
    print (c ^ 7);
  }
  return 0;
}
