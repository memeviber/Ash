func main(): int {
  let i: int = 0;
  while (i < 200) {
    let p: int* = alloc_ints(1);
    let j: int = 1;
    p[0] = i;
    while (j < 48) {
      p = grow_ints(p, j, j + 1);
      p[j] = i + j;
      j = j + 1;
    }
    if (p[47] == i + 47) then {
      print 1;
    } else {
      print 0;
    }
    free_ints(p);
    i = i + 1;
  }
  return 0;
}
