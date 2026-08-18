func main(): int {
  let a: int = 103;
  let b: int = 41;
  let restored: int = (a + b) - b;
  let mixed: int = (a ^ 11) << 0;
  if restored != 103 then return 1;
  if mixed != 108 then return 2;
  return 0;
}
