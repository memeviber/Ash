enum Event {
  Value {
    number: int;
  },
  Empty,
};

func main(): int {
  let event: Event = Event::Value(9);
  match event {
    Value => {
      return 0;
    }
    Empty => {
      return 1;
    }
  }
  return 2;
}
