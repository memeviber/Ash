struct Pair89 {
  left: int;
  right: int;
}
func main(): int {
  let p: Pair89 = 0;
  p.left = 89;
  p.right = (p.left + 2);
  print p.right;
  return 0;
}
