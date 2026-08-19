struct Box<T> {
  value: T;
  extra: int;
}

func main(): int {
  let zero: Box<int> = 0;
  let pointer: Box<int>* = memory_alloc(1, zero);
  pointer.extra = 7;
  if pointer.extra != 7 then return 1;
  memory_resize(pointer, 1, 3, zero);
  pointer.extra = 11;
  if pointer.extra != 11 then return 2;
  memory_free(pointer);
  return 0;
}