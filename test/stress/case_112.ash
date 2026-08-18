func main(): int {
  let p: int* = alloc_ints(8);
  let q: int* = p + 5;
  p[0] = 115;
  q[0] = 121;
  print *q;
  print q - p;
  free_ints(p);
  return 0;
}
