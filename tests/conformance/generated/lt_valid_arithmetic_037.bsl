func main(): int {
  let a: int = 278;
  let b: int = 116;
  let restored: int = (a + b) - b;
  let mixed: int = (a ^ 30) << 1;
  if restored != 278 then return 1;
  if mixed != 528 then return 2;
  return 0;
}
