struct Pair7 {
  left: int;
  right: double;
}

func main(): int {
  let p: Pair7 = 0;
  p.left = 17;
  p.right = 8.5;
  if p.left != 17 then return 1;
  if p.right != 8.5 then return 2;
  return 0;
}
