func main(): int {
  let p: int* = alloc_ints(8);
  p[0] = 45;
  p[5] = 46;
  let q: int* = p + 5;
  let d: int = q - p;
  print *q;
  print d;
  free_ints(p);
  return 0;
}
