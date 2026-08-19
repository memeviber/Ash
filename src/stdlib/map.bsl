namespace map {
  // Growable generic hash map backed by typed parallel bucket arrays.
  // Capacities are powers of two, so bucket selection uses hash & (cap - 1).
  // states: 0 = empty, 1 = occupied, 2 = tombstone.
  struct HashMap<K, V> {
    keys: K*;
    values: V*;
    states: int*;
    key_zero: K;
    value_zero: V;
    hasher: fn(K): int;
    equals: fn(K, K): bool;
    len: int;
    used: int;
    cap: int;
  }

  func power_of_two_at_least(value: int): int {
    let cap: int = 8;
    if value < 8 then return cap;
    while cap < value {
      let grown: int = cap * 2;
      if grown < cap then return cap;
      cap = grown;
    }
    return cap;
  }

  func with_capacity<K, V>(capacity: int, key_zero: K, value_zero: V): HashMap<K, V> {
    let m: HashMap<K, V> = 0;
    let cap: int = map::power_of_two_at_least(capacity);
    m.keys = memory_alloc(cap, key_zero);
    m.values = memory_alloc(cap, value_zero);
    m.states = memory_alloc(cap, 0);
    m.key_zero = key_zero;
    m.value_zero = value_zero;
    m.hasher = 0;
    m.equals = 0;
    m.len = 0;
    m.used = 0;
    m.cap = cap;
    return m;
  }

  func new<K, V>(key_zero: K, value_zero: V): HashMap<K, V> {
    return map::with_capacity(8, key_zero, value_zero);
  }

  func with_hasher<K, V>(capacity: int, key_zero: K, value_zero: V, hasher: fn(K): int, equals: fn(K, K): bool): HashMap<K, V> {
    let m: HashMap<K, V> = map::with_capacity(capacity, key_zero, value_zero);
    m.hasher = hasher;
    m.equals = equals;
    return m;
  }

  func new_with_hasher<K, V>(key_zero: K, value_zero: V, hasher: fn(K): int, equals: fn(K, K): bool): HashMap<K, V> {
    return map::with_hasher(8, key_zero, value_zero, hasher, equals);
  }

  func bucket_index(capacity: int, hash: int): int {
    if capacity < 1 then return 0;
    return hash & (capacity - 1);
  }

  func find<K, V>(m: HashMap<K, V>, key: K): int {
    let hash: int = 0;
    if m.hasher != 0 then hash = (m.hasher)(key);
    let start: int = map::bucket_index(m.cap, hash);
    let mask: int = m.cap - 1;
    let i: int = 0;
    while i < m.cap {
      let slot: int = (start + i) & mask;
      if m.states[slot] == 0 then return (0 - 1);
      if m.states[slot] == 1 then {
        let same: bool = false;
        if m.equals != 0 then same = (m.equals)(m.keys[slot], key);
        else same = m.keys[slot] == key;
        if same then return slot;
      }
      i = i + 1;
    }
    return (0 - 1);
  }

  func insert_raw<K, V>(m: HashMap<K, V>, key: K, value: V): HashMap<K, V> {
    let hash: int = 0;
    if m.hasher != 0 then hash = (m.hasher)(key);
    let start: int = map::bucket_index(m.cap, hash);
    let mask: int = m.cap - 1;
    let i: int = 0;
    while i < m.cap {
      let slot: int = (start + i) & mask;
      if m.states[slot] != 1 then {
        if m.states[slot] == 0 then m.used = m.used + 1;
        m.keys[slot] = key;
        m.values[slot] = value;
        m.states[slot] = 1;
        m.len = m.len + 1;
        return m;
      }
      i = i + 1;
    }
    return m;
  }

  func rehash<K, V>(m: HashMap<K, V>, minimum: int): HashMap<K, V> {
    let next: HashMap<K, V> = map::with_capacity(minimum, m.key_zero, m.value_zero);
    next.hasher = m.hasher;
    next.equals = m.equals;
    let old_index: int = 0;
    while old_index < m.cap {
      if m.states[old_index] == 1 then {
        let hash: int = 0;
        if next.hasher != 0 then hash = (next.hasher)(m.keys[old_index]);
        let start: int = map::bucket_index(next.cap, hash);
        let mask: int = next.cap - 1;
        let probe: int = 0;
        while probe < next.cap {
          let slot: int = (start + probe) & mask;
          if next.states[slot] == 0 then {
            next.keys[slot] = m.keys[old_index];
            next.values[slot] = m.values[old_index];
            next.states[slot] = 1;
            next.len = next.len + 1;
            next.used = next.used + 1;
            probe = next.cap;
          } else probe = probe + 1;
        }
      }
      old_index = old_index + 1;
    }
    memory_free(m.keys);
    memory_free(m.values);
    memory_free(m.states);
    return next;
  }

  func reserve<K, V>(m: HashMap<K, V>, minimum: int): HashMap<K, V> {
    let target: int = map::power_of_two_at_least(minimum);
    if target < m.cap && m.used * 4 < m.cap * 3 then return m;
    if target == m.cap && m.used * 4 < m.cap * 3 then return m;
    return map::rehash(m, target);
  }

  func ensure_capacity<K, V>(m: HashMap<K, V>, required: int): HashMap<K, V> {
    if required < 1 then return m;
    let target: int = m.cap;
    while target * 3 < required * 4 {
      let grown: int = target * 2;
      if grown < target then return map::rehash(m, target);
      target = grown;
    }
    if target > m.cap then return map::reserve(m, target);
    if m.used * 4 > m.cap * 3 || m.used * 4 == m.cap * 3 then return map::reserve(m, m.cap);
    return m;
  }

  func put<K, V>(m: HashMap<K, V>, key: K, value: V): HashMap<K, V> {
    let found: int = map::find(m, key);
    if found > (0 - 1) then { m.values[found] = value; return m; }
    m = map::ensure_capacity(m, m.len + 1);
    return map::insert_raw(m, key, value);
  }

  func contains_key<K, V>(m: HashMap<K, V>, key: K): bool {
    return map::find(m, key) > (0 - 1);
  }

  func get_or<K, V>(m: HashMap<K, V>, key: K, fallback: V): V {
    let found: int = map::find(m, key);
    if found > (0 - 1) then return m.values[found];
    return fallback;
  }

  func remove<K, V>(m: HashMap<K, V>, key: K): HashMap<K, V> {
    let found: int = map::find(m, key);
    if found > (0 - 1) then {
      m.states[found] = 2;
      m.len = m.len - 1;
    }
    return m;
  }

  func clear<K, V>(m: HashMap<K, V>): HashMap<K, V> {
    let i: int = 0;
    while i < m.cap { m.states[i] = 0; i = i + 1; }
    m.len = 0;
    m.used = 0;
    return m;
  }

  func length<K, V>(m: HashMap<K, V>): int { return m.len; }
  func capacity<K, V>(m: HashMap<K, V>): int { return m.cap; }

  func free<K, V>(m: HashMap<K, V>): HashMap<K, V> {
    if m.keys != 0 then memory_free(m.keys);
    if m.values != 0 then memory_free(m.values);
    if m.states != 0 then memory_free(m.states);
    m.keys = 0;
    m.values = 0;
    m.states = 0;
    m.hasher = 0;
    m.equals = 0;
    m.len = 0;
    m.used = 0;
    m.cap = 0;
    return m;
  }
}
