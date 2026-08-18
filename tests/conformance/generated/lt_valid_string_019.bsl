include "../../../src/stdlib/string.bsl"
func main(): int {
  let text: string = "λ";
  if str::utf8_validate(text) == false then return 1;
  if str::byte_len(text) != 2 then return 2;
  if str::codepoint_len(text) != 1 then return 3;
  let joined: string = str::concat(text, "!");
  if str::byte_len(joined) != 3 then return 4;
  return 0;
}
