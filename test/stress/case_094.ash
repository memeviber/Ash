struct Pair94 {
  left: int;
  right: int;
}
func main(): int {
  let p: Pair94 = 0;
  p.left = 94;
  p.right = (p.left + 2);
  print p.right;
  return 0;
}
