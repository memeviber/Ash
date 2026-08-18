include "../../../src/stdlib/array.bsl"
func main(): int {
  let a: array::Array<int> = array::new(1, 0);
  a = array::push(a, "wrong", 0);
  return 0;
}
