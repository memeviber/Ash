func main(): int {
  let first: array<int> = array_make(1);
  let second: array<int> = first;
  print second[0];
  return 0;
}

// Expected: compile-time rejection because an owning dynamic array cannot be copied implicitly.
