func main(): int {
  let p: int* = alloc_ints(16);
  p[7] = 269;
  let q: int* = p + 7;
  let distance: int = q - p;
  let observed: int = *q;
  free_ints(p);
  if distance != 7 then return 1;
  if observed != 269 then return 2;
  return 0;
}
