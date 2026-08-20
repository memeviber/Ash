enum Message {
  Move {
    x: int;
    y: int;
  }
}

func main(): int {
  let moved: Message = Message::Move(1);
  return 0;
}
