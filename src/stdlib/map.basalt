namespace map {
  // Growable generic hash map. The table uses typed parallel storage, linear
  // probing and tombstones, so K and V can be any Basalt value type supported
  // by equality and typed memory allocation.
  struct HashMap<K, V> {
    keys: K*;
    values: V*;
    states: int*;
    key_zero: K;
    value_zero: V;
    hasher: fn(K): int;
    equals: fn(K, K): bool;
    len: int;
    cap: int;
  }

  func with_capacity<K, V>(capacity: int, key_zero: K, value_zero: V): HashMap<K, V> {
    let m: HashMap<K, V> = 0;
    let cap: int = capacity;
    if cap < 8 then cap = 8;
    m.keys = memory_alloc(cap, key_zero);
    m.values = memory_alloc(cap, value_zero);
    m.states = memory_alloc(cap, 0);
    m.key_zero = key_zero;
    m.value_zero = value_zero;
    m.hasher = 0;
    m.equals = 0;
    m.len = 0;
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

  func find<K, V>(m: HashMap<K, V>, key: K): int {
    let start: int = 0;
    if m.hasher != 0 then {
      start = (m.hasher)(key);
      if start < 0 then start = 0 - start;
      start = start % m.cap;
    }
    let i: int = 0;
    while i < m.cap {
      let slot: int = (start + i) % m.cap;
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
    let start: int = 0;
    if m.hasher != 0 then {
      start = (m.hasher)(key);
      if start < 0 then start = 0 - start;
      start = start % m.cap;
    }
    let i: int = 0;
    while i < m.cap {
      let slot: int = (start + i) % m.cap;
      if m.states[slot] != 1 then {
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

  func reserve<K, V>(m: HashMap<K, V>, minimum: int): HashMap<K, V> {
    if minimum < 8 then return m;
    if m.cap > minimum then return m;
    if m.cap == minimum then return m;
    let next: HashMap<K, V> = map::with_capacity(minimum, m.key_zero, m.value_zero);
    next.hasher = m.hasher;
    next.equals = m.equals;
    let i: int = 0;
    while i < m.cap {
      if m.states[i] == 1 then {
        let start: int = 0;
        if next.hasher != 0 then {
          start = (next.hasher)(m.keys[i]);
          if start < 0 then start = 0 - start;
          start = start % next.cap;
        }
        let j: int = 0;
        while j < next.cap {
          let slot: int = (start + j) % next.cap;
          if next.states[slot] != 1 then {
            next.keys[slot] = m.keys[i];
            next.values[slot] = m.values[i];
            next.states[slot] = 1;
            next.len = next.len + 1;
            j = next.cap;
          } else { j = j + 1; }
        }
      }
      i = i + 1;
    }
    memory_free(m.keys);
    memory_free(m.values);
    memory_free(m.states);
    return next;
  }

  func ensure_capacity<K, V>(m: HashMap<K, V>, required: int): HashMap<K, V> {
    if required < 1 then return m;
    if m.cap * 3 > required * 4 then return m;
    if m.cap * 3 == required * 4 then return m;
    let next: int = m.cap;
    if next < 8 then next = 8;
    while next * 3 < required * 4 {
      let grown: int = next * 2;
      if grown < next then { grown = required * 2; }
      next = grown;
    }
    return map::reserve(m, next);
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
    m.cap = 0;
    return m;
  }
}
