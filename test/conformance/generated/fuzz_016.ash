func main(): int {
  let p: int* = alloc_ints(8);
  p[0] = 17;
  p[2] = 18;
  let q: int* = p + 2;
  let d: int = q - p;
  print *q;
  print d;
  free_ints(p);
  return 0;
}
