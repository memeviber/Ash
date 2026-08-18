struct Pair0 {
  left: int;
  right: double;
}

func main(): int {
  let p: Pair0 = 0;
  p.left = 10;
  p.right = 1.5;
  if p.left != 10 then return 1;
  if p.right != 1.5 then return 2;
  return 0;
}
