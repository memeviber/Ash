namespace array {
  func for_each<T>(a: array::Array<T>, visit: fn(T): void): void {
    let i: int = 0;
    while i < a.len {
      visit(a.data[i]);
      i = i + 1;
    }
  }

  func any<T>(a: array::Array<T>, predicate: fn(T): bool): bool {
    let i: int = 0;
    while i < a.len {
      if predicate(a.data[i]) then return true;
      i = i + 1;
    }
    return false;
  }

  func all<T>(a: array::Array<T>, predicate: fn(T): bool): bool {
    let i: int = 0;
    while i < a.len {
      if predicate(a.data[i]) == false then return false;
      i = i + 1;
    }
    return true;
  }

  func fold<T, U>(a: array::Array<T>, initial: U, combine: fn(U, T): U): U {
    let result: U = initial;
    let i: int = 0;
    while i < a.len {
      result = combine(result, a.data[i]);
      i = i + 1;
    }
    return result;
  }
}

namespace slice {
  func for_each<T>(s: slice::Slice<T>, visit: fn(T): void): void {
    let i: int = 0;
    while i < s.len {
      visit(s.data[i]);
      i = i + 1;
    }
  }

  func any<T>(s: slice::Slice<T>, predicate: fn(T): bool): bool {
    let i: int = 0;
    while i < s.len {
      if predicate(s.data[i]) then return true;
      i = i + 1;
    }
    return false;
  }

  func all<T>(s: slice::Slice<T>, predicate: fn(T): bool): bool {
    let i: int = 0;
    while i < s.len {
      if predicate(s.data[i]) == false then return false;
      i = i + 1;
    }
    return true;
  }

  func fold<T, U>(s: slice::Slice<T>, initial: U, combine: fn(U, T): U): U {
    let result: U = initial;
    let i: int = 0;
    while i < s.len {
      result = combine(result, s.data[i]);
      i = i + 1;
    }
    return result;
  }
}

namespace map {
  func for_each<K, V>(m: map::HashMap<K, V>, visit: fn(K, V): void): void {
    let i: int = 0;
    while i < m.cap {
      if m.states[i] == 1 then {
        visit(m.keys[i], m.values[i]);
      }
      i = i + 1;
    }
  }
}
