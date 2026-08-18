struct Pair3 {
  left: int;
  right: double;
}

func main(): int {
  let p: Pair3 = 0;
  p.left = 13;
  p.right = 4.5;
  if p.left != 13 then return 1;
  if p.right != 4.5 then return 2;
  return 0;
}
