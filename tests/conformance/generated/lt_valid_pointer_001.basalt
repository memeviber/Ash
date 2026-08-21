func main(): int {
  let p: int* = alloc_ints(16);
  p[2] = 22;
  let q: int* = p + 2;
  let distance: int = q - p;
  let observed: int = *q;
  free_ints(p);
  if distance != 2 then return 1;
  if observed != 22 then return 2;
  return 0;
}
