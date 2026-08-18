struct Pair79 {
  left: int;
  right: int;
}
func main(): int {
  let p: Pair79 = 0;
  p.left = 79;
  p.right = (p.left + 2);
  print p.right;
  return 0;
}
