struct Pair39 {
  left: int;
  right: int;
}
func main(): int {
  let p: Pair39 = 0;
  p.left = 39;
  p.right = (p.left + 2);
  print p.right;
  return 0;
}
