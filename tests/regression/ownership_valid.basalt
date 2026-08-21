func main(): int {
  let p: int* = alloc_ints(4);
  p[0] = 21;
  p = grow_ints(p, 4, 8);
  p[7] = 21;
  print p[0] + p[7];
  free_ints(p);
  return 0;
}
