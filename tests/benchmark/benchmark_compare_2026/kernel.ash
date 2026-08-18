extern func alloc_ints(size: int): int*;
extern func free_ints(p: int*): void;

func main(): int {
  let n: int = 1000000;
  let data: int* = alloc_ints(n);
  let i: int = 0;
  while i < n {
    data[i] = (i * 17 + 23) - ((i * 17 + 23) / 1009) * 1009;
    i = i + 1;
  }

  let sum: int = 0;
  let hash: int = 0;
  i = 0;
  while i < n {
    let value: int = data[i];
    sum = (sum + value * value) - ((sum + value * value) / 1000003) * 1000003;
    hash = (hash * 33 + value) - ((hash * 33 + value) / 1000003) * 1000003;
    i = i + 1;
  }

  print sum;
  print hash;
  free_ints(data);
  return 0;
}
