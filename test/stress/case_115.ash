func main(): int {
  let a: int = (((17 >> 1) / 2) * (58 << 2));
  let b: int = (((0 - 2) | 12) + 3);
  let c: int = (a + b);
  if (c & 1) == 0 then {
    print c;
  } else {
    print (c ^ 7);
  }
  return 0;
}
