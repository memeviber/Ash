include "../../../src/stdlib/array.bsl"
func main(): int {
  let values: array::Array<bool> = array::new(1, false);
  let j: int = 0;
  while j < 18 {
    values = array::push(values, true, false);
    j = j + 1;
  }
  if array::length(values) != 18 then return 1;
  if array::capacity(values) < 18 then return 2;
  values = array::set(values, 7, true);
  if array::length(values) != 18 then return 3;
  values = array::free(values);
  return 0;
}
