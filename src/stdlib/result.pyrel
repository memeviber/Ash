namespace result {
  struct Result<T, E> {
    ok: int;
    value: T;
    error: E;
  }

  func ok<T, E>(value: T, error_zero: E): Result<T, E> {
    let r: Result<T, E> = 0;
    r.ok = 1;
    r.value = value;
    r.error = error_zero;
    return r;
  }

  func err<T, E>(value_zero: T, error: E): Result<T, E> {
    let r: Result<T, E> = 0;
    r.ok = 0;
    r.value = value_zero;
    r.error = error;
    return r;
  }

  func is_ok<T, E>(r: Result<T, E>): int { return r.ok; }
  func is_err<T, E>(r: Result<T, E>): int { return r.ok == 0; }

  func unwrap_or<T, E>(r: Result<T, E>, fallback: T): T {
    if r.ok == 1 then { return r.value; }
    return fallback;
  }

  func value_or<T, E>(r: Result<T, E>, fallback: T): T {
    return unwrap_or(r, fallback);
  }

  func error_or<T, E>(r: Result<T, E>, fallback: E): E {
    if r.ok == 0 then { return r.error; }
    return fallback;
  }

  struct Option<T> {
    present: int;
    value: T;
  }

  func some<T>(value: T): Option<T> {
    let o: Option<T> = 0;
    o.present = 1;
    o.value = value;
    return o;
  }

  func none<T>(zero: T): Option<T> {
    let o: Option<T> = 0;
    o.present = 0;
    o.value = zero;
    return o;
  }

  func is_some<T>(o: Option<T>): int { return o.present; }
  func is_none<T>(o: Option<T>): int { return o.present == 0; }

  func unwrap_or_option<T>(o: Option<T>, fallback: T): T {
    if o.present == 1 then { return o.value; }
    return fallback;
  }

  func value_or_option<T>(o: Option<T>, fallback: T): T {
    return unwrap_or_option(o, fallback);
  }
}
