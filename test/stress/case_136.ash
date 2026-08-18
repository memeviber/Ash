func main(): int {
  let i: int = 0;
  let acc: int = 0;
  while (i < 4) {
    if ((i & 1) == 0) then {
      acc = acc + (i << 1);
    } else {
      acc = acc ^ i;
    }
    i = i + 1;
  }
  print acc;
  return 0;
}
