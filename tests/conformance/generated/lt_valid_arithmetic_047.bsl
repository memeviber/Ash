func main(): int {
  let a: int = 348;
  let b: int = 146;
  let restored: int = (a + b) - b;
  let mixed: int = (a ^ 12) << 3;
  if restored != 348 then return 1;
  if mixed != 2688 then return 2;
  return 0;
}
