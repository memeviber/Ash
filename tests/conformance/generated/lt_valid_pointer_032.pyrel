func main(): int {
  let p: int* = alloc_ints(16);
  p[5] = 425;
  let q: int* = p + 5;
  let distance: int = q - p;
  let observed: int = *q;
  free_ints(p);
  if distance != 5 then return 1;
  if observed != 425 then return 2;
  return 0;
}
