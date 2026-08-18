struct Pair64 {
  left: int;
  right: int;
}
func main(): int {
  let p: Pair64 = 0;
  p.left = 64;
  p.right = (p.left + 2);
  print p.right;
  return 0;
}
