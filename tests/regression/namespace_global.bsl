namespace demo {
  let answer: int = 42;
  const meaning: int = 7;
}

func main(): int {
  if demo::answer != 42 then return 1;
  if demo::meaning != 7 then return 2;
  return 0;
}