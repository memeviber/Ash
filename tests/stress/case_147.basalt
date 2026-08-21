func main(): int {
  let p: int* = alloc_ints(8);
  let q: int* = p + 4;
  p[0] = 150;
  q[0] = 156;
  print *q;
  print q - p;
  free_ints(p);
  return 0;
}
