func increment_int(value: int): int {
  return value + 1;
}

func shift_f32(value: f32): f32 {
  return value + 1.0;
}

func apply<T>(value: T, callback: fn(T): T): T {
  return callback(value);
}

func apply_borrowed<T>(value: T*, callback: fn(T): T): T {
  let snapshot: T = *value;
  return callback(snapshot);
}

func main(): int {
  let int_callback: fn(int): int = &increment_int;
  let value: int = 41;
  let applied: int = apply(value, int_callback);
  if applied != 42 then return 1;

  {
    let borrowed: int* = &value;
    let borrowed_result: int = apply_borrowed(borrowed, int_callback);
    if borrowed_result != 42 then return 2;
  }

  value = 43;
  if value != 43 then return 3;

  let f32_callback: fn(f32): f32 = &shift_f32;
  let amount: f32 = 4.5;
  let shifted: f32 = apply(amount, f32_callback);
  if shifted < 5.49 || shifted > 5.51 then return 4;

  return 0;
}
