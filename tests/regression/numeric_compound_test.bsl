func main(): int {
  let wide: long = 10;
  wide += 5;
  wide -= 3;
  wide *= 2;
  wide /= 3;
  wide %= 4;
  if wide != 0 then return 1;

  let wider: long long = 7;
  wider |= 2;
  wider &= 3;
  wider ^= 1;
  wider <<= 2;
  wider >>= 1;
  if wider != 4 then return 2;

  let real: float = 1.5;
  real += 2.25;
  real -= 0.75;
  real *= 2.0;
  real /= 2.0;
  if real < 2.99 || real > 3.01 then return 3;

  let precise: double = 9.0;
  precise /= 3.0;
  precise += 0.5;
  precise -= 0.5;
  if precise < 2.99 || precise > 3.01 then return 4;

  return 0;
}
