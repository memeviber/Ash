let cleanup_count: int = 0;

func mark(value: int): void {
  cleanup_count = (cleanup_count * 10) + value;
}

func normal(): void {
  let resource: int = 7;
  defer mark(1);
  defer mark(2);
  resource = resource + 1;
}

func early(): void {
  defer mark(3);
  return;
}

func main(): int {
  normal();
  if cleanup_count != 21 then return 1;
  early();
  if cleanup_count != 213 then return 2;
  return 0;
}
