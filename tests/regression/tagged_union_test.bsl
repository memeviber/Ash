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

func main(): int {
  let moved: Message = Message::Move(3, 4);
  if moved.Move.x != 3 then return 1;
  if moved.Move.y != 4 then return 2;

  let written: Message = Message::Write(42);
  if written.Write.code != 42 then return 3;

  let quit: Message = Message::Quit();
  if quit.tag < 0 then return 4;
  return 0;
}
