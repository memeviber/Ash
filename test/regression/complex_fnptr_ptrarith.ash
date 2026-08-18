func read_at(p: int*, index: int): int {
  return *(p + index);
}

func sum_range(p: int*, first: int, last: int): int {
  let i: int = first;
  let total: int = 0;
  while i < last {
    total = total + *(p + i);
    i = i + 1;
  }
  return total;
}

func main(): void {
  let values: int* = alloc_ints(6);
  *(values + 0) = 10;
  *(values + 1) = 20;
  *(values + 2) = 30;
  *(values + 3) = 40;
  *(values + 4) = 50;
  *(values + 5) = 60;

  let at: fn(int*, int): int = &read_at;
  let aggregate: fn(int*, int, int): int = &sum_range;
  let shifted: int* = 1 + values;
  let tail: int* = values + 5;
  let distance: int = tail - shifted;

  print at(shifted, 2);
  print aggregate(values, 1, 5);
  print distance;
  print *(tail - 2);
  free_ints(values);
}

