func main(): int {
  let p: int* = alloc_ints(8);
  p[0] = 33;
  p[3] = 34;
  let q: int* = p + 3;
  let d: int = q - p;
  print *q;
  print d;
  free_ints(p);
  return 0;
}
