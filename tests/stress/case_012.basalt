func main(): int {
  let p: int* = alloc_ints(8);
  let q: int* = p + 1;
  p[0] = 15;
  q[0] = 21;
  print *q;
  print q - p;
  free_ints(p);
  return 0;
}
