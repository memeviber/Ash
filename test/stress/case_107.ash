func main(): int {
  let p: int* = alloc_ints(8);
  let q: int* = p + 6;
  p[0] = 110;
  q[0] = 116;
  print *q;
  print q - p;
  free_ints(p);
  return 0;
}
