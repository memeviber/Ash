include "../../src/stdlib/slice.ash"
include "../../src/stdlib/result.ash"

func main(): int {
  let numbers: slice::Slice<int> = slice::new(0);
  let filled: slice::Slice<int> = slice::push(numbers, 10);
  let filled2: slice::Slice<int> = slice::push(filled, 20);
  let changed: slice::Slice<int> = slice::set(filled2, 1, 25);
  print slice::length(changed);
  print slice::get(changed, 0);
  print slice::get(changed, 1);
  print slice::get_or(changed, 99, 77);
  print slice::last_or(changed, 88);
  print slice::is_full(changed);
  let empty: slice::Slice<int> = slice::clear(changed);
  print slice::is_empty(empty);

  let words: slice::Slice<string> = slice::new("");
  let words2: slice::Slice<string> = slice::push(words, "ash");
  print slice::get(words2, 0);

  let good: result::Result<int, string> = result::ok(42, "zero");
  let bad: result::Result<int, string> = result::err(0, "failed");
  let present: result::Option<string> = result::some("yes");
  let absent: result::Option<string> = result::none("");
  print result::is_ok(good);
  print result::is_err(bad);
  print result::unwrap_or(good, 7);
  print result::error_or(bad, "fallback");
  print result::is_some(present);
  print result::is_none(absent);
  print result::unwrap_or_option(present, "no");
  print result::unwrap_or_option(absent, "no");
  return 0;
}
