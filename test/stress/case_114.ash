struct Pair114 {
  left: int;
  right: int;
}
func main(): int {
  let p: Pair114 = 0;
  p.left = 114;
  p.right = (p.left + 2);
  print p.right;
  return 0;
}
