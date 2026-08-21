struct Pair {
  left: int;
}

func main(): int {
  let zero: Pair = 0;
  let pointer: Pair* = memory_alloc(1, zero);
  pointer.left = 7;
  if pointer.left != 7 then return 1;
  memory_free(pointer);
  return 0;
}