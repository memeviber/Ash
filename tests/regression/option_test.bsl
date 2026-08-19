include "../../src/stdlib/option.bsl"

func main(): int {
  let present: option::Option<int> = option::some(42);
  let absent: option::Option<int> = option::none(0);
  print option::is_some(present);
  print option::is_none(absent);
  print option::is_none(present);
  print option::unwrap_or(present, 7);
  print option::unwrap_or(absent, 7);
  let s: option::Option<string> = option::some("hello");
  print option::unwrap_or(s, "fallback");
  print option::unwrap_or(option::none("zero"), "fallback");
  return 0;
}
