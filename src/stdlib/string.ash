func string_byte_len(s: string): int {
  let i: int = 0;
  while s[i] != '\0' {
    i = i + 1;
  }
  return i;
}

func string_at(s: string, index: int): int {
  return s[index] & 255;
}

func string_eq(a: string, b: string): int {
  let i: int = 0;
  while a[i] != '\0' && b[i] != '\0' {
    if a[i] != b[i] then { return 0; }
    i = i + 1;
  }
  return a[i] == b[i];
}

func string_concat(a: string, b: string): string {
  return a ++ b;
}


// UTF-8 helpers. Existing string_byte_len/string_at remain byte-oriented.

func string_utf8_decode_at(s: string, offset: int): int {
  let n: int = string_byte_len(s);
  if offset < 0 then { return (0 - 1); }
  if offset > (n - 1) then { return (0 - 1); }
  let b0: int = string_at(s, offset);
  if b0 < 128 then { return b0; }
  if b0 > 193 && b0 < 224 then {
    if offset + 1 > (n - 1) then { return (0 - 1); }
    let b1: int = string_at(s, offset + 1);
    if b1 < 128 || b1 > 191 then { return (0 - 1); }
    return (b0 - 192) * 64 + (b1 - 128);
  }
  if b0 > 223 && b0 < 240 then {
    if offset + 2 > (n - 1) then { return (0 - 1); }
    let b1: int = string_at(s, offset + 1);
    let b2: int = string_at(s, offset + 2);
    if b1 < 128 || b1 > 191 || b2 < 128 || b2 > 191 then { return (0 - 1); }
    let cp: int = (b0 - 224) * 4096 + (b1 - 128) * 64 + (b2 - 128);
    if cp < 2048 then { return (0 - 1); }
    if cp > 55295 && cp < 57344 then { return (0 - 1); }
    return cp;
  }
  if b0 > 239 && b0 < 245 then {
    if offset + 3 > (n - 1) then { return (0 - 1); }
    let b1: int = string_at(s, offset + 1);
    let b2: int = string_at(s, offset + 2);
    let b3: int = string_at(s, offset + 3);
    if b1 < 128 || b1 > 191 || b2 < 128 || b2 > 191 || b3 < 128 || b3 > 191 then { return (0 - 1); }
    let cp: int = (b0 - 240) * 262144 + (b1 - 128) * 4096 + (b2 - 128) * 64 + (b3 - 128);
    if cp < 65536 || cp > 1114111 then { return (0 - 1); }
    return cp;
  }
  return (0 - 1);
}

func string_utf8_next(s: string, offset: int): int {
  let cp: int = string_utf8_decode_at(s, offset);
  if cp < 0 then { return (0 - 1); }
  let b0: int = string_at(s, offset);
  if b0 < 128 then { return offset + 1; }
  if b0 < 224 then { return offset + 2; }
  if b0 < 240 then { return offset + 3; }
  return offset + 4;
}

func string_utf8_validate(s: string): bool {
  let n: int = string_byte_len(s);
  let offset: int = 0;
  while offset < n {
    offset = string_utf8_next(s, offset);
    if offset < 0 then { return false; }
  }
  return true;
}

func string_codepoint_len(s: string): int {
  let n: int = string_byte_len(s);
  let offset: int = 0;
  let count: int = 0;
  while offset < n {
    offset = string_utf8_next(s, offset);
    if offset < 0 then { return (0 - 1); }
    count = count + 1;
  }
  return count;
}

func string_codepoint_byte_offset(s: string, index: int): int {
  if index < 0 then { return (0 - 1); }
  let n: int = string_byte_len(s);
  let offset: int = 0;
  let count: int = 0;
  while offset < n {
    if count == index then { return offset; }
    offset = string_utf8_next(s, offset);
    if offset < 0 then { return (0 - 1); }
    count = count + 1;
  }
  if count == index then { return offset; }
  return (0 - 1);
}

func string_codepoint_at(s: string, index: int): int {
  let offset: int = string_codepoint_byte_offset(s, index);
  if offset < 0 then { return (0 - 1); }
  return string_utf8_decode_at(s, offset);
}

func string_utf8_self_test(): int {
  let ascii: string = "Ash";
  if string_utf8_validate(ascii) == false then { return 0; }
  if string_codepoint_len(ascii) != 3 then { return 0; }
  if string_codepoint_at(ascii, 1) != 115 then { return 0; }
  return 1;
}
