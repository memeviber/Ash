enum Color {
  Red,
  Green,
  Blue
}
let current: Color = Red;
func choose(c: Color): Color {
  return c;
}
func main(): void {
  current = Green;
  current = choose(Blue);
  print current;
}
