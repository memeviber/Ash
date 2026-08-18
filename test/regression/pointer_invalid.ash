func main(): void {
  let xs: int* = alloc_ints(2);
  let p: int* = xs;
  let bad: int = p + 1;
  print bad;
}
