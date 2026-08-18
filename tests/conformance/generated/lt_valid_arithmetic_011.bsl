func main(): int {
  let a: int = 96;
  let b: int = 38;
  let restored: int = (a + b) - b;
  let mixed: int = (a ^ 0) << 3;
  if restored != 96 then return 1;
  if mixed != 768 then return 2;
  return 0;
}
