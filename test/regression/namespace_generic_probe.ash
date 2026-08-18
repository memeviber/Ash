namespace result {
  struct Result<T, E> {
    value: T;
    error: E;
  }

  func value<T>(value: T): Result<T, int> {
    let out: Result<T, int> = 0;
    out.value = value;
    return out;
  }
}

func main(): int {
  let out: result::Result<int, int> = result::value(42);
  return out.value;
}
