struct Pair {
  left: int;
  right: int;
}

func update(p: Pair): Pair {
  p.left = 7;
  return p;
}

func main(): int {
  let p: Pair = 0;
  p = update(p);
  print p.left;
  return 0;
}
