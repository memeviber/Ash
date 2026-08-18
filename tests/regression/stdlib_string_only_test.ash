include "../../src/stdlib/string.ash"

func main(): int {
  let text: string = "Aé";
  print string_byte_len(text);
  print string_at(text, 0);
  print string_eq(text, "Aé");
  print string_eq(text, "Ax");
  return 0;
}
