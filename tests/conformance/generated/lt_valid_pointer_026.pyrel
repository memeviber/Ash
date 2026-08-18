func main(): int {
  let p: int* = alloc_ints(16);
  p[6] = 347;
  let q: int* = p + 6;
  let distance: int = q - p;
  let observed: int = *q;
  free_ints(p);
  if distance != 6 then return 1;
  if observed != 347 then return 2;
  return 0;
}
