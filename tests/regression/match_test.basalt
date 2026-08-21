enum Message {
  Quit,
  Move {
    x: int;
    y: int;
  },
  Write {
    code: int;
  },
};

func classify(message: Message): int {
  let result: int = 0;
  match message {
    Quit => {
      result = 1;
    }
    Move(x, y) => {
      result = (x * 10) + y;
    }
    Write(code) => {
      result = code;
    }
  }
  return result;
}

func main(): int {
  let quit: Message = Message::Quit();
  if classify(quit) != 1 then return 1;
  let moved: Message = Message::Move(3, 4);
  if classify(moved) != 34 then return 2;
  let written: Message = Message::Write(42);
  if classify(written) != 42 then return 3;
  return 0;
}
