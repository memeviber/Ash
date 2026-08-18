func main(): int {
  let a: int = 208;
  let b: int = 86;
  let restored: int = (a + b) - b;
  let mixed: int = (a ^ 48) << 3;
  if restored != 208 then return 1;
  if mixed != 1792 then return 2;
  return 0;
}
