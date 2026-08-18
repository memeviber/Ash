namespace alpha {
  namespace beta {
    namespace gamma {
      func run(): int {
        return 17;
      }
    }
    namespace delta {
      func run(): int {
        return 23;
      }
    }
  }
}

namespace infra {
  func basalt_memory_free(): int {
    return 29;
  }
}

func main(): int {
  let a: int = alpha::beta::gamma::run();
  let b: int = alpha::beta::delta::run();
  let c: int = infra::basalt_memory_free();
  print a;
  print b;
  print c;
  return 0;
}
