func main(): int {
  let p: int* = alloc_ints(8);
  let q: int* = p + 1;
  p[0] = 45;
  q[0] = 51;
  print *q;
  print q - p;
  free_ints(p);
  return 0;
}
