enum Color {
  Red,
  Green
}
enum Size {
  Small,
  Large
}
let current: Color = Red;
func take_color(c: Color): void {
  return;
}
func main(): void {
  current = 3;
  take_color(Small);
}
