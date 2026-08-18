func main(): int {
  let p: int* = alloc_ints(8);
  let q: int* = p + 6;
  p[0] = 50;
  q[0] = 56;
  print *q;
  print q - p;
  free_ints(p);
  return 0;
}
