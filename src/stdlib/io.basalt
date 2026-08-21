namespace io {
  // Reads at most max_len - 1 bytes and always returns a NUL-terminated string.
  // max_len must be in [2, 1048576]. The returned string is owned by the runtime.
  func read_line(max_len: int): string {
    return runtime_read_line(max_len);
  }

  // Reads one bounded decimal integer. On malformed, oversized, or EOF input,
  // returns fallback and exposes the reason through status().
  func read_int(fallback: int): int {
    return runtime_read_int(fallback);
  }

  // 0 = success, 1 = EOF with no data, 2 = malformed integer,
  // 3 = line/input too long (remaining input is discarded),
  // 4 = integer outside the int range.
  func status(): int {
    return runtime_io_status();
  }

  func write(value: string): void {
    runtime_write_string(value);
  }

  func writeln(value: string): void {
    runtime_write_line(value);
  }

  func write_int(value: int): void {
    runtime_write_int(value);
  }

  func write_char(value: char): void {
    runtime_write_char(value);
  }
}
