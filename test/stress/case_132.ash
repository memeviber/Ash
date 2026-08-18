func main(): int {
  let p: int* = alloc_ints(8);
  let q: int* = p + 1;
  p[0] = 135;
  q[0] = 141;
  print *q;
  print q - p;
  free_ints(p);
  return 0;
}
