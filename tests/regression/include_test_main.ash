include "include_test_lib.ash"
include "./include_test_lib.ash"
include "include_test_lib.ash"
includec "include_test_runtime.c"
func main(): void {
  print imported_value();
  print c_value();
}
