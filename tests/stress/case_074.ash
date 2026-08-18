struct Pair74 {
  left: int;
  right: int;
}
func main(): int {
  let p: Pair74 = 0;
  p.left = 74;
  p.right = (p.left + 2);
  print p.right;
  return 0;
}
