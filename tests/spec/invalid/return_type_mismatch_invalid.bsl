func returns_int(): int {
  return "wrong";
}

func main(): void {
  returns_int();
}

// diagnostic.code: 23
// diagnostic.expected: int
// diagnostic.found: string
// diagnostic.hint: return a value matching the function return type
// diagnostic.excerpt: return "wrong";

