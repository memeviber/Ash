include "../../src/stdlib/result.bsl"

func main(): int {
  let failures: int = 0;

  if 17 % 5 != 2 then { failures = failures + 1; }
  if 100 % 7 != 2 then { failures = failures + 1; }
  if 0 % 5 != 0 then { failures = failures + 1; }

  let n: int = 37;
  if n % 1 != 0 then { failures = failures + 1; }
  if n % n != 0 then { failures = failures + 1; }
  if 1 % n != 1 then { failures = failures + 1; }

  let i: int = 0;
  let residue_sum: int = 0;
  while i < 100 {
    residue_sum = residue_sum + (i % 3);
    i = i + 1;
  }
  if residue_sum != 99 then { failures = failures + 1; }

  let a: int = 23;
  let b: int = 11;
  let c: int = 6;
  let d: int = 4;
  if (a + b) % c != 4 then { failures = failures + 1; }
  if a % b + c % d != 3 then { failures = failures + 1; }
  if (a * 3 + b) % (c + 1) != 3 then { failures = failures + 1; }
  if (0 - 17) % 5 != (0 - 2) then { failures = failures + 1; }

  let ok: result::Result<int, string> = result::ok(42 % 10, "unused");
  if result::unwrap_or(ok, 0) != 2 then { failures = failures + 1; }

  return failures;
}
