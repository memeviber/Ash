include "include_test_lib.bsl"
include "./include_test_lib.bsl"
include "include_test_lib.bsl"
includec "include_test_runtime.c"

extern func c_value(): int;
func main(): void {
  print imported_value();
  print c_value();
}
