includec "macro_pointer_runtime.c"

struct Box {
  value: int;
  next: Box*;
}

func pointer_add(p: int*): int {
  return *p + 2;
}

func main(): void {
  let x: int = 40;
  let p: int* = &x;
  print pointer_add(p);
  print macro_scale(*p);
  print macro_bias(*p, 1);
}
