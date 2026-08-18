include "../../src/stdlib/result.bsl"

func safe_div_int(a: int, b: int): result::Result<int, int> {
  if b == 0 then { return result::err(0, 1); }
  return result::ok(a / b, 0);
}

func main(): int {
  let ok: result::Result<int, int> = safe_div_int(20, 4);
  let bad: result::Result<int, int> = safe_div_int(20, 0);
  let some: result::Option<int> = result::some(7);
  let none: result::Option<int> = result::none(0);
  print result::is_ok(ok);
  print result::is_err(bad);
  print result::unwrap_or(ok, 99);
  print result::error_or(bad, 99);
  print result::is_some(some);
  print result::is_none(none);
  print result::unwrap_or_option(some, 99);
  print result::unwrap_or_option(none, 99);
  return 0;
}
