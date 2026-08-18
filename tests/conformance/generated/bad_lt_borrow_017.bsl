include "../../../src/stdlib/slice.bsl"
func main(): int {
  let s: slice::Slice<double> = slice::new(0.0);
  s = slice::set(s, 0, 'x');
  return slice::length(s);
}
