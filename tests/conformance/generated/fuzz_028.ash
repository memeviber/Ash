func main(): int {
  let p: int* = alloc_ints(8);
  p[0] = 29;
  p[4] = 30;
  let q: int* = p + 4;
  let d: int = q - p;
  print *q;
  print d;
  free_ints(p);
  return 0;
}
