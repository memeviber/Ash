func main(): int {
  let p: int* = alloc_ints(8);
  let q: int* = p + 5;
  p[0] = 85;
  q[0] = 91;
  print *q;
  print q - p;
  free_ints(p);
  return 0;
}
