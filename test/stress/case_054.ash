struct Pair54 {
  left: int;
  right: int;
}
func main(): int {
  let p: Pair54 = 0;
  p.left = 54;
  p.right = (p.left + 2);
  print p.right;
  return 0;
}
