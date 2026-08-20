struct Cell {
  value: int;
}

let calls: int = 0;
let index_calls: int = 0;

func next_index(): int {
  index_calls += 1;
  return 0;
}

func main(): int {
  let zero: Cell = 0;
  let pointer: Cell* = memory_alloc(1, zero);
  pointer.value = 10;
  pointer.value += 5;
  if pointer.value != 15 then return 1;

  let values: int[2] = 0;
  values[0] = 20;
  values[1] = 99;
  values[next_index()] += 10;
  if index_calls != 1 then return 2;
  if values[0] != 30 || values[1] != 99 then return 3;

  pointer.value += 2 * 3;
  if pointer.value != 21 then return 4;

  memory_free(pointer);
  return 0;
}
