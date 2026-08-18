include "../../../src/stdlib/slice.bsl"
func main(): int {
  let s: slice::Slice<double> = slice::new(0.0);
  s = slice::push(s, 'x');
  return 0;
}
