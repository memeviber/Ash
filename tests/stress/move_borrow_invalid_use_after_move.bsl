func consume_owner(values: array<int>): int {
  array_free(values);
  return 0;
}

func main(): int {
  let values: array<int> = array_make(2);
  values = array_push(values, 11);
  consume_owner(values);
  print values[0];
  return 0;
}
