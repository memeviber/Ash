func main(): int {
  let p: int* = alloc_ints(8);
  let q: int* = p + 3;
  p[0] = 155;
  q[0] = 161;
  print *q;
  print q - p;
  free_ints(p);
  return 0;
}
