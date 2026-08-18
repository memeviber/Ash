func main(): int {
  let a: int = ((((0 - 9) - 2) & 15) ^ 1);
  let b: int = ((9 ^ 15) ^ 21);
  let c: int = (a + b);
  if (c & 1) == 0 then {
    print c;
  } else {
    print (c ^ 7);
  }
  return 0;
}
