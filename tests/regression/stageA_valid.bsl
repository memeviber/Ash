const MAX: int = 100;
const LETTER: char = 'A';
let empty: int* = null;
func main(): void {
  let x: int = 1;
  {
    let shadow: int = 2;
    print shadow;
  }
  {
    let shadow: int = 3;
    print shadow;
  }
  print LETTER;
  print empty == null;
  print MAX;
}
