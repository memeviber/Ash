func main(): int {
  let p: int* = alloc_ints(16);
  p[1] = 100;
  let q: int* = p + 1;
  let distance: int = q - p;
  let observed: int = *q;
  free_ints(p);
  if distance != 1 then return 1;
  if observed != 100 then return 2;
  return 0;
}
