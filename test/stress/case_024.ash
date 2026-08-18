struct Pair24 {
  left: int;
  right: int;
}
func main(): int {
  let p: Pair24 = 0;
  p.left = 24;
  p.right = (p.left + 2);
  print p.right;
  return 0;
}
