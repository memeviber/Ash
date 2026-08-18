func main(): int {
  let a: int = (((17 >> 1) / 5) | 3);
  let b: int = (((0 - 7) + 0) | 11);
  let c: int = (a + b);
  if (c & 1) == 0 then {
    print c;
  } else {
    print (c ^ 7);
  }
  return 0;
}
