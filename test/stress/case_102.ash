func main(): int {
  let p: int* = alloc_ints(8);
  let q: int* = p + 1;
  p[0] = 105;
  q[0] = 111;
  print *q;
  print q - p;
  free_ints(p);
  return 0;
}
