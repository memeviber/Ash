namespace option {
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

  func unwrap_or<T>(o: Option<T>, fallback: T): T {
    if o.present == 1 then { return o.value; }
    return fallback;
  }
}
