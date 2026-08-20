include "../../src/stdlib/io.bsl"

func main(): int {
  let line: string = io::read_line(1);
  if line[0] == 0 then { return 0; }
  return 1;
}
