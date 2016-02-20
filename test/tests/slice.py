# output: ok

class Listy:
    def __init__(self, values):
      self.values = values
    def __getitem__(self, x):
        return self.values[x]
    def __setitem__(self, x, y):
        self.values[x] = y

# test subscript reference

data = [0, 1, 2]
assert data[0] == 0
assert data[2] == 2
assert data[-2] == 1

data2 = Listy([1, 2, 3])
assert data2[0] == 1
assert data2[2] == 3
assert data2[-2] == 2

# test subscript assignment

data = [0, 1, 2]
data[0] = 3
assert data == [3, 1, 2]
data[-2] = 0
assert data == [3, 0, 2]

data2 = Listy([1, 2, 3])
data2[0] = 4
assert data2.values == [4, 2, 3]
data2[-2] = 1
assert data2.values == [4, 1, 3]

# test subscript reference with slice

data = [0, 1, 2]

assert [][0:0] == []
assert [][0:1] == []

assert data[0:0] == []
assert data[0:1] == [0]
assert data[0:2] == [0, 1]
assert data[1:2] == [1]
assert data[2:2] == []

assert data[1:-1] == [1]
assert data[1:0] == []
assert data[1:-10] == []

assert data[1:3] == [1, 2]
assert data[2:4] == [2]

assert data[0:3:2] == [0, 2]
assert data[1:3:2] == [1]
assert data[0:2:-1] == []
assert data[3:0:-1] == [2, 1]
assert data[4:0:-1] == [2, 1]
assert data[3:0:-2] == [2]
assert data[4:0:-2] == [2]

assert data[:] == [0, 1, 2]
assert data[1:] == [1, 2]
assert data[:2] == [0, 1]

assert data[::] == [0, 1, 2]
assert data[1::] == [1, 2]
assert data[:2:] == [0, 1]
assert data[::2] == [0, 2]

print('ok')
