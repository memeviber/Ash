struct Point {
  x: int;
  y: int;
}
let a: Point = 0;
let b: Point = 0;
func main(): void {
  a.x = 7;
  b = a;
  print b.x;
}
