include "include_test_nested.ash"
func imported_value(): int {
  return nested_value() + 1;
}
