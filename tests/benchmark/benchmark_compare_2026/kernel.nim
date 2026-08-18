const n = 1_000_000

var data = newSeq[int](n)
var i = 0
while i < n:
  data[i] = (i * 17 + 23) mod 1009
  i += 1

var sum = 0
var hash = 0
i = 0
while i < n:
  let value = data[i]
  sum = (sum + value * value) mod 1_000_003
  hash = (hash * 33 + value) mod 1_000_003
  i += 1

stdout.writeLine(sum)
stdout.writeLine(hash)
