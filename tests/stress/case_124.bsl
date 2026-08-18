struct Pair124 {
  left: int;
  right: int;
}
func main(): int {
  let p: Pair124 = 0;
  p.left = 124;
  p.right = (p.left + 2);
  print p.right;
  return 0;
}
