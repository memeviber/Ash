func main(): int {
  let a: int = (33 >> 1);
  let b: int = ((2 >> 1) * (5 - 3));
  let c: int = (a + b);
  if (c & 1) == 0 then {
    print c;
  } else {
    print (c ^ 7);
  }
  return 0;
}
