func main(): int {
  let p: int* = alloc_ints(16);
  p[4] = 412;
  let q: int* = p + 4;
  let distance: int = q - p;
  let observed: int = *q;
  free_ints(p);
  if distance != 4 then return 1;
  if observed != 412 then return 2;
  return 0;
}
