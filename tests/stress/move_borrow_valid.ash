func consume_owner(values: array<int>): int {
  print values[0];
  array_free(values);
  return 0;
}

func borrow_read(): int {
  let value: int = 7;
  {
    let first: int* = &value;
    let copied: int = *first;
    print copied;
  }
  value = 8;
  print value;
  return 0;
}

func move_owner(): int {
  let values: array<int> = array_make(2);
  values = array_push(values, 41);
  print values[0];
  consume_owner(values);
  return 0;
}

func main(): int {
  borrow_read();
  move_owner();
  return 0;
}

// Expected output: 7, 8, 41, 41.
// The owner is moved exactly once into `consume_owner`; the scalar read through
// a borrow is allowed, while the borrowed pointer is confined to this scope.

