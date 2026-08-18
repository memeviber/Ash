func main(): int {
  let a: int = (((9 & 23) * (49 >> 1)) + (48 >> 1));
  let b: int = ((2 - 3) - (15 << 1));
  let c: int = (a + b);
  if (c & 1) == 0 then {
    print c;
  } else {
    print (c ^ 7);
  }
  return 0;
}
