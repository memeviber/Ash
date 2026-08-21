enum Color {
  Red,
  Blue,
};

func score(color: Color): int {
  match color {
    Red => { return 10; }
    Blue => { return 20; }
  }
  return 0;
}

func main(): int {
  if score(Color::Red()) != 10 then return 1;
  if score(Color::Blue()) != 20 then return 2;
  return 0;
}

