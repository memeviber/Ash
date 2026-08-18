includec "asymmetric_deep_macro_runtime.c"

extern func deep_macro(x: int): int;

struct Leaf {
  value: int;
}

struct LeftBranch {
  leaf: Leaf*;
  weight: int;
}

struct RightBranch {
  left: LeftBranch*;
  marker: int;
  spare: Leaf*;
}

struct AsymmetricTree {
  left: LeftBranch*;
  right: RightBranch*;
  depth: int;
  payload: Leaf*;
}

enum ShapeKind { Thin, Wide, Forked }

let tree: AsymmetricTree = 0;

func pointer_transform(p: int*): int {
  return *p + 5;
}

func main(): void {
  let value: int = 7;
  let ptr: int* = &value;
  print tree.depth;
  print tree.left == 0;
  print tree.right == 0;
  print pointer_transform(ptr);
  print deep_macro(2);
}
