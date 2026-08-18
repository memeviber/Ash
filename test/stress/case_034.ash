struct Pair34 {
  left: int;
  right: int;
}
func main(): int {
  let p: Pair34 = 0;
  p.left = 34;
  p.right = (p.left + 2);
  print p.right;
  return 0;
}
