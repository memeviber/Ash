namespace map {
  func new(x: int): int {
    return x + 1;
  }
}

func main(): int {
  let value: int = map::new(41);
  return value;
}
