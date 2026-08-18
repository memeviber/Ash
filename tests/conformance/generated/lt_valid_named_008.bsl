struct Pair8 {
  left: int;
  right: double;
}

func main(): int {
  let p: Pair8 = 0;
  p.left = 18;
  p.right = 9.5;
  if p.left != 18 then return 1;
  if p.right != 9.5 then return 2;
  return 0;
}
