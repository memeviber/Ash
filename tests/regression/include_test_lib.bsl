include "include_test_nested.bsl"
func imported_value(): int {
  return nested_value() + 1;
}
