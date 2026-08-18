func main(): int {
  let p: int* = alloc_ints(8);
  p[0] = 21;
  p[1] = 22;
  let q: int* = p + 1;
  let d: int = q - p;
  print *q;
  print d;
  free_ints(p);
  return 0;
}
