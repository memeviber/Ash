func main(): int {
  let p: int* = alloc_ints(2);
  let q: double* = p;
  free_ints(p);
  return 0;
}
