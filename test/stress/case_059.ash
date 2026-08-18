struct Pair59 {
  left: int;
  right: int;
}
func main(): int {
  let p: Pair59 = 0;
  p.left = 59;
  p.right = (p.left + 2);
  print p.right;
  return 0;
}
