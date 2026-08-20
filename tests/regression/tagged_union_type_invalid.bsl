enum Message {
  Move {
    x: int;
    y: int;
  }
}

func main(): int {
  let moved: Message = Message::Move("wrong", 2);
  return 0;
}
