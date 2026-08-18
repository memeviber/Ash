struct Pair84 {
  left: int;
  right: int;
}
func main(): int {
  let p: Pair84 = 0;
  p.left = 84;
  p.right = (p.left + 2);
  print p.right;
  return 0;
}
