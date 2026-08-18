func main(): int {
  let a: int = 47;
  let b: int = 17;
  let restored: int = (a + b) - b;
  let mixed: int = (a ^ 51) << 0;
  if restored != 47 then return 1;
  if mixed != 28 then return 2;
  return 0;
}
