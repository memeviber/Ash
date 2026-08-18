func main(): int {
  let p: int* = alloc_ints(8);
  p[0] = 37;
  p[2] = 38;
  let q: int* = p + 2;
  let d: int = q - p;
  print *q;
  print d;
  free_ints(p);
  return 0;
}
