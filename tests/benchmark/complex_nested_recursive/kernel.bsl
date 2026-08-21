include "../../../src/stdlib/array.bsl"

struct Matrix {
  cells: array::Array<int>;
  row_seeds: array::Array<int>;
  rows: int;
  columns: int;
}

struct Workspace {
  primary: Matrix;
  secondary: Matrix;
  rounds: int;
}

func deep_mix(value: int, depth: int): int {
  if depth == 0 then return value;
  let mixed: int = (value * 33 + depth) - ((value * 33 + depth) / 1000003) * 1000003;
  return deep_mix(mixed, depth - 1);
}

func make_matrix(rows: int, columns: int, salt: int): Matrix {
  let matrix: Matrix = 0;
  matrix.cells = array::new(rows * columns, 0);
  matrix.row_seeds = array::new(rows, 0);
  matrix.rows = rows;
  matrix.columns = columns;
  let row: int = 0;
  while row < rows {
    matrix.row_seeds = array::push(matrix.row_seeds, row * 17 + salt, 0);
    let column: int = 0;
    while column < columns {
      let index: int = row * columns + column;
      let value: int = (index * 13 + salt) - ((index * 13 + salt) / 997) * 997;
      matrix.cells = array::push(matrix.cells, value, 0);
      column = column + 1;
    }
    row = row + 1;
  }
  return matrix;
}

func checksum_matrix(matrix: Matrix, depth: int): int {
  let checksum: int = 0;
  let row: int = 0;
  while row < matrix.rows {
    let row_seed: int = array::get(matrix.row_seeds, row);
    let mixed_seed: int = deep_mix(row_seed, depth);
    checksum = (checksum + mixed_seed) - ((checksum + mixed_seed) / 1000003) * 1000003;
    let column: int = 0;
    while column < matrix.columns {
      let index: int = row * matrix.columns + column;
      let value: int = array::get(matrix.cells, index);
      let contribution: int = value * 3 + row + column;
      checksum = (checksum + contribution) - ((checksum + contribution) / 1000003) * 1000003;
      column = column + 1;
    }
    row = row + 1;
  }
  return checksum;
}

func free_matrix(matrix: Matrix): void {
  array::free(matrix.cells);
  array::free(matrix.row_seeds);
}

func main(): int {
  let workspace: Workspace = 0;
  workspace.primary = make_matrix(20000, 32, 11);
  workspace.secondary = make_matrix(20000, 32, 29);
  workspace.rounds = 128;
  let left: int = checksum_matrix(workspace.primary, workspace.rounds);
  let right: int = checksum_matrix(workspace.secondary, workspace.rounds);
  print left;
  print right;
  free_matrix(workspace.primary);
  free_matrix(workspace.secondary);
  return 0;
}
