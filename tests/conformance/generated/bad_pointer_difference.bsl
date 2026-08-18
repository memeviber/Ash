func main(): int {
  let a: int* = alloc_ints(2);
  let b: char* = null;
  let n: int = a - b;
  free_ints(a);
  return n;
}
