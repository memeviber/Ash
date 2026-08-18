struct Pair9 {
  left: int;
  right: int;
}
func main(): int {
  let p: Pair9 = 0;
  p.left = 9;
  p.right = (p.left + 2);
  print p.right;
  return 0;
}
