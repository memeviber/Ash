include "../../src/stdlib/array.bsl"

struct Cell<T> {
  value: T;
}

struct Packet<T> {
  cell: Cell<T>;
  samples: array::Array<T>;
}

func keep_nested<T>(packet: Packet<T>, value: T): T {
  if packet.samples.len == (0 - 1) then return value;
  return value;
}

func main(): int {
  let packet32: Packet<f32> = 0;
  let value32: f32 = keep_nested(packet32, 3.5);
  if value32 < 3.49 || value32 > 3.51 then return 1;

  let packet64: Packet<f64> = 0;
  let value64: f64 = 8.25;
  let result64: f64 = keep_nested(packet64, value64);
  if result64 < 8.24 || result64 > 8.26 then return 2;

  return 0;
}
