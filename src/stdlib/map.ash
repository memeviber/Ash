namespace map {
  // Generic fixed-inline map. It supports any Ash value type and keeps a
  // predictable bounded representation until generic dynamic storage exists.
  struct HashMap<K, V> {
    key0: K;
    value0: V;
    used0: int;
    key1: K;
    value1: V;
    used1: int;
    key2: K;
    value2: V;
    used2: int;
    key3: K;
    value3: V;
    used3: int;
    key4: K;
    value4: V;
    used4: int;
    key5: K;
    value5: V;
    used5: int;
    key6: K;
    value6: V;
    used6: int;
    key7: K;
    value7: V;
    used7: int;
    len: int;
    cap: int;
  }

  func new<K, V>(key_zero: K, value_zero: V): HashMap<K, V> {
    let m: HashMap<K, V> = 0;
    m.key0 = key_zero;
    m.value0 = value_zero;
    m.key1 = key_zero;
    m.value1 = value_zero;
    m.key2 = key_zero;
    m.value2 = value_zero;
    m.key3 = key_zero;
    m.value3 = value_zero;
    m.key4 = key_zero;
    m.value4 = value_zero;
    m.key5 = key_zero;
    m.value5 = value_zero;
    m.key6 = key_zero;
    m.value6 = value_zero;
    m.key7 = key_zero;
    m.value7 = value_zero;
    m.cap = 8;
    m.len = 0;
    return m;
  }

  func put<K, V>(m: HashMap<K, V>, key: K, value: V): HashMap<K, V> {
    if m.used0 == 1 && m.key0 == key then { m.value0 = value; return m; }
    if m.used1 == 1 && m.key1 == key then { m.value1 = value; return m; }
    if m.used2 == 1 && m.key2 == key then { m.value2 = value; return m; }
    if m.used3 == 1 && m.key3 == key then { m.value3 = value; return m; }
    if m.used4 == 1 && m.key4 == key then { m.value4 = value; return m; }
    if m.used5 == 1 && m.key5 == key then { m.value5 = value; return m; }
    if m.used6 == 1 && m.key6 == key then { m.value6 = value; return m; }
    if m.used7 == 1 && m.key7 == key then { m.value7 = value; return m; }
    if m.used0 == 0 then { m.used0 = 1; m.key0 = key; m.value0 = value; m.len = m.len + 1; return m; }
    if m.used1 == 0 then { m.used1 = 1; m.key1 = key; m.value1 = value; m.len = m.len + 1; return m; }
    if m.used2 == 0 then { m.used2 = 1; m.key2 = key; m.value2 = value; m.len = m.len + 1; return m; }
    if m.used3 == 0 then { m.used3 = 1; m.key3 = key; m.value3 = value; m.len = m.len + 1; return m; }
    if m.used4 == 0 then { m.used4 = 1; m.key4 = key; m.value4 = value; m.len = m.len + 1; return m; }
    if m.used5 == 0 then { m.used5 = 1; m.key5 = key; m.value5 = value; m.len = m.len + 1; return m; }
    if m.used6 == 0 then { m.used6 = 1; m.key6 = key; m.value6 = value; m.len = m.len + 1; return m; }
    if m.used7 == 0 then { m.used7 = 1; m.key7 = key; m.value7 = value; m.len = m.len + 1; return m; }
    return m;
  }

  func contains_key<K, V>(m: HashMap<K, V>, key: K): int {
    if m.used0 == 1 && m.key0 == key then { return 1; }
    if m.used1 == 1 && m.key1 == key then { return 1; }
    if m.used2 == 1 && m.key2 == key then { return 1; }
    if m.used3 == 1 && m.key3 == key then { return 1; }
    if m.used4 == 1 && m.key4 == key then { return 1; }
    if m.used5 == 1 && m.key5 == key then { return 1; }
    if m.used6 == 1 && m.key6 == key then { return 1; }
    if m.used7 == 1 && m.key7 == key then { return 1; }
    return 0;
  }

  func get_or<K, V>(m: HashMap<K, V>, key: K, fallback: V): V {
    if m.used0 == 1 && m.key0 == key then { return m.value0; }
    if m.used1 == 1 && m.key1 == key then { return m.value1; }
    if m.used2 == 1 && m.key2 == key then { return m.value2; }
    if m.used3 == 1 && m.key3 == key then { return m.value3; }
    if m.used4 == 1 && m.key4 == key then { return m.value4; }
    if m.used5 == 1 && m.key5 == key then { return m.value5; }
    if m.used6 == 1 && m.key6 == key then { return m.value6; }
    if m.used7 == 1 && m.key7 == key then { return m.value7; }
    return fallback;
  }

  func remove<K, V>(m: HashMap<K, V>, key: K): HashMap<K, V> {
    if m.used0 == 1 && m.key0 == key then { m.used0 = 0; m.len = m.len - 1; return m; }
    if m.used1 == 1 && m.key1 == key then { m.used1 = 0; m.len = m.len - 1; return m; }
    if m.used2 == 1 && m.key2 == key then { m.used2 = 0; m.len = m.len - 1; return m; }
    if m.used3 == 1 && m.key3 == key then { m.used3 = 0; m.len = m.len - 1; return m; }
    if m.used4 == 1 && m.key4 == key then { m.used4 = 0; m.len = m.len - 1; return m; }
    if m.used5 == 1 && m.key5 == key then { m.used5 = 0; m.len = m.len - 1; return m; }
    if m.used6 == 1 && m.key6 == key then { m.used6 = 0; m.len = m.len - 1; return m; }
    if m.used7 == 1 && m.key7 == key then { m.used7 = 0; m.len = m.len - 1; return m; }
    return m;
  }

  func clear<K, V>(m: HashMap<K, V>): HashMap<K, V> {
    m.used0 = 0; m.used1 = 0; m.used2 = 0; m.used3 = 0;
    m.used4 = 0; m.used5 = 0; m.used6 = 0; m.used7 = 0;
    m.len = 0;
    return m;
  }

  func length<K, V>(m: HashMap<K, V>): int { return m.len; }
  func capacity<K, V>(m: HashMap<K, V>): int { return m.cap; }

  // Small generic pair map kept for source compatibility with early Ash code.
  struct Map<T> {
    first: T;
    second: T;
    len: int;
  }

  func map<T>(m: Map<T>): Map<T> {
    let next: Map<T> = 0;
    next.first = m.first;
    next.second = m.second;
    next.len = m.len;
    return next;
  }

  // Full dynamically-resized int-to-int map used where unbounded storage is
  // required before generic dynamic allocation is available.
  struct IntMap {
    keys: int*;
    values: int*;
    states: int*;
    cap: int;
    len: int;
  }

  func new_int(capacity: int): IntMap {
    let m: IntMap = 0;
    m.cap = 8;
    while m.cap < capacity { m.cap = m.cap * 2; }
    m.keys = alloc_ints(m.cap);
    m.values = alloc_ints(m.cap);
    m.states = alloc_ints(m.cap);
    m.len = 0;
    return m;
  }

  func index_int(m: IntMap, key: int): int {
    let i: int = key & (m.cap - 1);
    let n: int = 0;
    while n < m.cap {
      if m.states[i] == 0 then { return (0 - 1); }
      if m.states[i] == 1 && m.keys[i] == key then { return i; }
      i = i + 1;
      if i > (m.cap - 1) then { i = 0; }
      n = n + 1;
    }
    return (0 - 1);
  }

  func insert_raw_int(m: IntMap, key: int, value: int): IntMap {
    let i: int = key & (m.cap - 1);
    while m.states[i] == 1 { i = i + 1; if i > (m.cap - 1) then { i = 0; } }
    m.keys[i] = key;
    m.values[i] = value;
    m.states[i] = 1;
    m.len = m.len + 1;
    return m;
  }

  func resize_int(m: IntMap, new_cap: int): IntMap {
    let next: IntMap = new_int(new_cap);
    let i: int = 0;
    while i < m.cap {
      if m.states[i] == 1 then { next = insert_raw_int(next, m.keys[i], m.values[i]); }
      i = i + 1;
    }
    free_ints(m.keys);
    free_ints(m.values);
    free_ints(m.states);
    return next;
  }

  func put_int(m: IntMap, key: int, value: int): IntMap {
    let i: int = index_int(m, key);
    if i > (0 - 1) then { m.values[i] = value; return m; }
    if (m.len + 1) * 4 > m.cap * 3 then { m = resize_int(m, m.cap * 2); }
    return insert_raw_int(m, key, value);
  }

  func get_int(m: IntMap, key: int, fallback: int): int {
    let i: int = index_int(m, key);
    if i < 0 then { return fallback; }
    return m.values[i];
  }

  func contains_int(m: IntMap, key: int): bool { return index_int(m, key) > (0 - 1); }

  func remove_int(m: IntMap, key: int): bool {
    let i: int = index_int(m, key);
    if i < 0 then { return false; }
    m.states[i] = 2;
    m.len = m.len - 1;
    return true;
  }

  func clear_int(m: IntMap): IntMap {
    let i: int = 0;
    while i < m.cap { m.states[i] = 0; i = i + 1; }
    m.len = 0;
    return m;
  }

  func length_int(m: IntMap): int { return m.len; }
  func capacity_int(m: IntMap): int { return m.cap; }

  func free_int(m: IntMap): void {
    free_ints(m.keys);
    free_ints(m.values);
    free_ints(m.states);
  }

  func self_test_int(): int {
    let m: IntMap = new_int(2);
    m = put_int(m, 1, 10);
    m = put_int(m, 9, 90);
    if get_int(m, 9, (0 - 1)) != 90 then { free_int(m); return 0; }
    if remove_int(m, 1) == false then { free_int(m); return 0; }
    if contains_int(m, 1) then { free_int(m); return 0; }
    free_int(m);
    return 1;
  }

  func ok_int(): bool { return self_test_int() == 1; }
}
