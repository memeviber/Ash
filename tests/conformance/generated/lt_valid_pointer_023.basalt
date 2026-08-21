func main(): int {
  let p: int* = alloc_ints(16);
  p[3] = 308;
  let q: int* = p + 3;
  let distance: int = q - p;
  let observed: int = *q;
  free_ints(p);
  if distance != 3 then return 1;
  if observed != 308 then return 2;
  return 0;
}
