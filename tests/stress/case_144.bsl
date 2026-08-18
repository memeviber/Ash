struct Pair144 {
  left: int;
  right: int;
}
func main(): int {
  let p: Pair144 = 0;
  p.left = 144;
  p.right = (p.left + 2);
  print p.right;
  return 0;
}
