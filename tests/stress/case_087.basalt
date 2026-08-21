func main(): int {
  let p: int* = alloc_ints(8);
  let q: int* = p + 4;
  p[0] = 90;
  q[0] = 96;
  print *q;
  print q - p;
  free_ints(p);
  return 0;
}
