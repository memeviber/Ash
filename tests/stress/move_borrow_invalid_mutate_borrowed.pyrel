func main(): int {
  let values: array<int> = array_make(1);
  let borrowed: array<int>* = &values;
  values = array_push(values, 7);
  print borrowed.len;
  return 0;
}

// Expected: compile-time rejection because values is mutated while borrowed.
