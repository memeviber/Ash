func main(): void {
  let xs: int* = alloc_ints(4);
  let p: int* = xs;
  let q: int* = p + 2;
  let r: int* = 1 + p;
  let s: int* = q - 1;
  let d: int = q - p;
  print d;
  print *s;
  print *r;
}
