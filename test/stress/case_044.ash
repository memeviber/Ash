struct Pair44 {
  left: int;
  right: int;
}
func main(): int {
  let p: Pair44 = 0;
  p.left = 44;
  p.right = (p.left + 2);
  print p.right;
  return 0;
}
