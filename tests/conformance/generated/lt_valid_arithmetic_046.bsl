func main(): int {
  let a: int = 341;
  let b: int = 143;
  let restored: int = (a + b) - b;
  let mixed: int = (a ^ 1) << 2;
  if restored != 341 then return 1;
  if mixed != 1360 then return 2;
  return 0;
}
