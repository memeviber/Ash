struct Pair139 {
  left: int;
  right: int;
}
func main(): int {
  let p: Pair139 = 0;
  p.left = 139;
  p.right = (p.left + 2);
  print p.right;
  return 0;
}
