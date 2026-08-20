#!/usr/bin/env python3
"""Generate a deterministic long-term Basalt conformance corpus.

The generated cases are deliberately small and independent so the Bootstrap
compiler can compile them repeatedly in the conformance runner.  They are not random fuzz inputs: every
family has an explicit semantic target and stable expected result.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "tests" / "conformance" / "generated"


def write(name: str, body: str) -> None:
    path = OUT / name
    path.write_text(body.rstrip() + "\n", encoding="utf-8")


def gen_arithmetic() -> None:
    for i in range(50):
        a = i * 7 + 19
        b = i * 3 + 5
        mask = (i * 11 + 7) & 63
        xor_value = a ^ mask
        shift = i % 4
        shifted = xor_value << shift
        write(
            f"lt_valid_arithmetic_{i:03d}.bsl",
            f"""func main(): int {{
  let a: int = {a};
  let b: int = {b};
  let restored: int = (a + b) - b;
  let mixed: int = (a ^ {mask}) << {shift};
  if restored != {a} then return 1;
  if mixed != {shifted} then return 2;
  return 0;
}}""",
        )


def gen_pointers() -> None:
    for i in range(35):
        offset = (i % 7) + 1
        value = i * 13 + 9
        write(
            f"lt_valid_pointer_{i:03d}.bsl",
            f"""func main(): int {{
  let p: int* = alloc_ints(16);
  p[{offset}] = {value};
  let q: int* = p + {offset};
  let distance: int = q - p;
  let observed: int = *q;
  free_ints(p);
  if distance != {offset} then return 1;
  if observed != {value} then return 2;
  return 0;
}}""",
        )


def gen_function_pointers() -> None:
    for i in range(35):
        left = i + 2
        right = i * 2 + 3
        expected = left + right + i
        write(
            f"lt_valid_fnptr_{i:03d}.bsl",
            f"""func add_{i}(a: int, b: int): int {{
  return a + b + {i};
}}

func main(): int {{
  let f: fn(int, int): int = &add_{i};
  let result: int = f({left}, {right});
  if result != {expected} then return 1;
  return 0;
}}""",
        )


def gen_arrays() -> None:
    cases = [
        ("int", "0", "17", "17"),
        ("bool", "false", "true", "true"),
        ("char", "'?'", "'q'", "'q'"),
        ("double", "0.0", "3.5", "3.5"),
        ("string", '""', '"basalt"', '"basalt"'),
    ]
    for i in range(20):
        typ, zero, value, expected = cases[i % len(cases)]
        write(
            f"lt_valid_array_{i:03d}.bsl",
            f"""include "../../../src/stdlib/array.bsl"
func main(): int {{
  let values: array::Array<{typ}> = array::new(1, {zero});
  let j: int = 0;
  while j < 18 {{
    values = array::push(values, {value}, {zero});
    j = j + 1;
  }}
  if array::length(values) != 18 then return 1;
  if array::capacity(values) < 18 then return 2;
  values = array::set(values, 7, {value});
  if array::length(values) != 18 then return 3;
  values = array::free(values);
  return 0;
}}""",
        )


def gen_slices() -> None:
    cases = [
        ("int", "0", "23"),
        ("bool", "false", "true"),
        ("char", "'!'", "'s'"),
        ("double", "0.0", "4.25"),
        ("string", '""', '"slice"'),
    ]
    for i in range(20):
        typ, zero, value = cases[i % len(cases)]
        write(
            f"lt_valid_slice_{i:03d}.bsl",
            f"""include "../../../src/stdlib/slice.bsl"
func main(): int {{
  let values: slice::Slice<{typ}> = slice::new({zero});
  let j: int = 0;
  while j < 21 {{
    values = slice::push(values, {value});
    j = j + 1;
  }}
  if slice::length(values) != 21 then return 1;
  if slice::capacity(values) < 21 then return 2;
  if slice::is_empty(values) then return 3;
  values = slice::set(values, 9, {value});
  values = slice::free(values);
  return 0;
}}""",
        )


def gen_maps() -> None:
    cases = [
        ("int", "double", "0", "0.0", "13", "2.5"),
        ("char", "bool", "'?'", "false", "'k'", "true"),
        ("double", "char", "0.0", "'?'", "6.5", "'m'"),
        ("bool", "int", "false", "0", "true", "31"),
    ]
    for i in range(20):
        key_type, value_type, key_zero, value_zero, key, value = cases[i % len(cases)]
        write(
            f"lt_valid_map_{i:03d}.bsl",
            f"""include "../../../src/stdlib/map.bsl"
func main(): int {{
  let m: map::HashMap<{key_type}, {value_type}> = map::new({key_zero}, {value_zero});
  m = map::put(m, {key}, {value});
  if map::length(m) != 1 then return 1;
  if map::contains_key(m, {key}) == false then return 2;
  m = map::remove(m, {key});
  if map::length(m) != 0 then return 3;
  m = map::free(m);
  return 0;
}}""",
        )


def gen_strings() -> None:
    cases = [
        ("Basalt", 6, 6),
        ("Aé", 3, 2),
        ("Δx", 3, 2),
        ("héllo", 6, 5),
        ("λ", 2, 1),
    ]
    for i in range(20):
        text, byte_len, codepoint_len = cases[i % len(cases)]
        write(
            f"lt_valid_string_{i:03d}.bsl",
            f"""include "../../../src/stdlib/string.bsl"
func main(): int {{
  let text: string = "{text}";
  if str::utf8_validate(text) == false then return 1;
  if str::byte_len(text) != {byte_len} then return 2;
  if str::codepoint_len(text) != {codepoint_len} then return 3;
  let joined: string = str::concat(text, "!");
  if str::byte_len(joined) != {byte_len + 1} then return 4;
  return 0;
}}""",
        )


def gen_named() -> None:
    for i in range(10):
        write(
            f"lt_valid_named_{i:03d}.bsl",
            f"""struct Pair{i} {{
  left: int;
  right: double;
}}

func main(): int {{
  let p: Pair{i} = 0;
  p.left = {i + 10};
  p.right = {i + 1}.5;
  if p.left != {i + 10} then return 1;
  if p.right != {i + 1}.5 then return 2;
  return 0;
}}""",
        )


def gen_invalid() -> None:
    for i in range(30):
        kind = i % 5
        if kind == 0:
            body = '''include "../../../src/stdlib/array.bsl"
func main(): int {
  let a: array::Array<int> = array::new(1, 0);
  a = array::push(a, "wrong", 0);
  return 0;
}'''
        elif kind == 1:
            body = '''include "../../../src/stdlib/slice.bsl"
func main(): int {
  let s: slice::Slice<double> = slice::new(0.0);
  s = slice::push(s, 'x');
  return 0;
}'''
        elif kind == 2:
            body = '''include "../../../src/stdlib/map.bsl"
func main(): int {
  let m: map::HashMap<int, int> = map::new(0, 0);
  m = map::put(m, "wrong", 1);
  return 0;
}'''
        elif kind == 3:
            body = '''func returns_int(): int {
  return "wrong";
}
func main(): int {
  return returns_int();
}'''
        else:
            body = '''func add_one(x: int): int {
  return x + 1;
}
func main(): int {
  let f: fn(int): int = &add_one;
  let result: int = f(1, 2);
  return result;
}'''
        write(f"bad_lt_type_{i:03d}.bsl", body)

    for i in range(20):
        write(
            f"bad_lt_owner_{i:03d}.bsl",
            '''include "../../../src/stdlib/array.bsl"
func main(): int {
  let a: array::Array<int> = array::new(2, 0);
  a = array::set(a, 0, "wrong");
  return array::length(a);
}''',
        )

    for i in range(20):
        write(
            f"bad_lt_borrow_{i:03d}.bsl",
            '''include "../../../src/stdlib/slice.bsl"
func main(): int {
  let s: slice::Slice<double> = slice::new(0.0);
  s = slice::set(s, 0, 'x');
  return slice::length(s);
}''',
        )

    for i in range(20):
        write(
            f"bad_lt_pointer_{i:03d}.bsl",
            '''func main(): int {
  let p: int* = alloc_ints(2);
  let q: double* = p;
  free_ints(p);
  return 0;
}''',
        )


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    gen_arithmetic()
    gen_pointers()
    gen_function_pointers()
    gen_arrays()
    gen_slices()
    gen_maps()
    gen_strings()
    gen_named()
    gen_invalid()
    print("generated 300 long-term conformance fixtures")


if __name__ == "__main__":
    main()
