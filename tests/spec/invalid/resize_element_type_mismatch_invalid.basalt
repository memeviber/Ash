func main(): void {
  let values: int* = alloc_ints(2);
  let wrong: string = "wrong";
  memory_resize(values, 2, 3, wrong);
}

// diagnostic.code: 36
// diagnostic.expected: int
// diagnostic.found: string
// diagnostic.hint: use an element type matching the existing array
// diagnostic.excerpt: memory_resize(values, 2, 3, wrong);

