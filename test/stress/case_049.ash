struct Pair49 {
  left: int;
  right: int;
}
func main(): int {
  let p: Pair49 = 0;
  p.left = 49;
  p.right = (p.left + 2);
  print p.right;
  return 0;
}
