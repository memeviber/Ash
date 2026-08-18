struct Pair119 {
  left: int;
  right: int;
}
func main(): int {
  let p: Pair119 = 0;
  p.left = 119;
  p.right = (p.left + 2);
  print p.right;
  return 0;
}
