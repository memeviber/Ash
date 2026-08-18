struct HM<K, V> {
  key: K;
  value: V;
  used: int;
}

func hm_make<K, V>(key_zero: K, value_zero: V): HM<K, V> {
  let m: HM<K, V> = 0;
  m.key = key_zero;
  m.value = value_zero;
  m.used = 0;
  return m;
}

func main(): int {
  let m: HM<int, int> = hm_make(3, 42);
  print m.value;
  return 0;
}
