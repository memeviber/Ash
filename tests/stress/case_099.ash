struct Pair99 {
  left: int;
  right: int;
}
func main(): int {
  let p: Pair99 = 0;
  p.left = 99;
  p.right = (p.left + 2);
  print p.right;
  return 0;
}
