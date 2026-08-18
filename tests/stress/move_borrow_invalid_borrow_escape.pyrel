func leak_local(): int* {
  let value: int = 99;
  return &value;
}

func main(): int {
  let leaked: int* = leak_local();
  print *leaked;
  return 0;
}

// Expected: compile-time rejection because the local borrow escapes leak_local.
