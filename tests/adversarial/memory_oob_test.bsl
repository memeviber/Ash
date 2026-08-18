include "../../src/stdlib/array.bsl"

// Bounds-safety test: checked accessors must reject an invalid index without
// reading or writing outside the owned allocation.
func main(): int {
  let a: array::Array<int> = array::new(1, 0);
  a = array::push(a, 42, 0);
  print array::get_or(a, 1, -1);
  if array::set_if_in_bounds(a, 1, 99) then {
    print 1;
  } else {
    print 0;
  }
  array::free(a);
  return 0;
}
