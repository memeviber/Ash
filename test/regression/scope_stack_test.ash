func main(): void {
  let x: int = 1;
  {
    let x: int = 2;
    print x;
  }
  {
    let x: int = 3;
    print x;
  }
  print x;
}
