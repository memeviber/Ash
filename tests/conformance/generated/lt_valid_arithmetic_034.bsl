func main(): int {
  let a: int = 257;
  let b: int = 107;
  let restored: int = (a + b) - b;
  let mixed: int = (a ^ 61) << 2;
  if restored != 257 then return 1;
  if mixed != 1264 then return 2;
  return 0;
}
