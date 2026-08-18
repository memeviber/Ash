func main(): int {
  let a: int = 292;
  let b: int = 122;
  let restored: int = (a + b) - b;
  let mixed: int = (a ^ 52) << 3;
  if restored != 292 then return 1;
  if mixed != 2176 then return 2;
  return 0;
}
