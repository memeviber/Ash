func main(): int {
  let p: int* = alloc_ints(8);
  let q: int* = p + 2;
  p[0] = 130;
  q[0] = 136;
  print *q;
  print q - p;
  free_ints(p);
  return 0;
}
