struct Pair129 {
  left: int;
  right: int;
}
func main(): int {
  let p: Pair129 = 0;
  p.left = 129;
  p.right = (p.left + 2);
  print p.right;
  return 0;
}
