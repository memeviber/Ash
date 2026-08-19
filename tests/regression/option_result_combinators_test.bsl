include "../../src/stdlib/option.bsl"
include "../../src/stdlib/result.bsl"

func inc(value: int): int {
  return value + 1;
}

func positive(value: int): bool {
  return value > 0;
}

func double_value(value: int): int {
  return value * 2;
}

func error_plus_one(value: int): int {
  return value + 1;
}

func main(): int {
  let status: int = 0;
  let present: option::Option<int> = option::some(4);
  let absent: option::Option<int> = option::none(0);
  let mapped: option::Option<int> = option::map(present, &inc, 0);
  let filtered: option::Option<int> = option::filter(mapped, &positive, 0);
  let rejected: option::Option<int> = option::filter(absent, &positive, 0);
  if option::is_some(filtered) == 0 then status = 1;
  if option::unwrap_or(filtered, 0) != 5 then status = 2;
  if option::is_none(rejected) == 0 then status = 3;
  if option::contains(filtered, 5) == false then status = 4;
  if option::contains(filtered, 4) then status = 5;

  let ok_value: result::Result<int, int> = result::ok(6, 0);
  let err_value: result::Result<int, int> = result::err(0, 7);
  let mapped_ok: result::Result<int, int> = result::map(ok_value, &double_value, 0);
  let mapped_err: result::Result<int, int> = result::map(err_value, &double_value, 0);
  let mapped_error: result::Result<int, int> = result::map_error(mapped_err, &error_plus_one, 0);
  if result::unwrap_or(mapped_ok, 0) != 12 then status = 6;
  if result::is_err(mapped_err) == 0 then status = 7;
  if result::error_or(mapped_error, 0) != 8 then status = 8;
  return status;
}
