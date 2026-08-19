func main(): int {
  let values: array<int> = array_make(1);
  array_free(values);
  array_free(values);
  return 0;
}
