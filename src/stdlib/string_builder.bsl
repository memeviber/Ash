namespace string_builder {
  struct Builder {
    data: char*;
    len: int;
    cap: int;
  }

  func new(): Builder {
    let b: Builder = 0;
    b.cap = 16;
    b.data = memory_alloc(b.cap, '\0');
    b.data[0] = '\0';
    return b;
  }

  func ensure(b: Builder, required: int): Builder {
    let target: int = b.cap;
    while target < required {
      target = target * 2;
    }
    if target != b.cap then {
      b.data = memory_resize(b.data, b.cap, target, '\0');
      b.cap = target;
    }
    return b;
  }

  func push_char(b: Builder, value: char): Builder {
    b = string_builder::ensure(b, b.len + 2);
    b.data[b.len] = value;
    b.len = b.len + 1;
    b.data[b.len] = '\0';
    return b;
  }

  func append(b: Builder, value: string): Builder {
    let i: int = 0;
    let n: int = str::byte_len(value);
    while i < n {
      b = string_builder::push_char(b, value[i]);
      i = i + 1;
    }
    return b;
  }

  func length(b: Builder): int { return b.len; }
  func capacity(b: Builder): int { return b.cap; }
  // Returns a non-owning pointer to the NUL-terminated buffer.
  func view(b: Builder): char* { return b.data; }

  func clear(b: Builder): Builder {
    b.len = 0;
    b.data[0] = '\0';
    return b;
  }

  func free(b: Builder): Builder {
    memory_free(b.data);
    b.data = 0;
    b.len = 0;
    b.cap = 0;
    return b;
  }
}
