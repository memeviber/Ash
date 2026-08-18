struct Pair29 {
  left: int;
  right: int;
}
func main(): int {
  let p: Pair29 = 0;
  p.left = 29;
  p.right = (p.left + 2);
  print p.right;
  return 0;
}
