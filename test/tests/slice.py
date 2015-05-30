# output: ok

assert [][0:0] == []
assert [][0:1] == []

data = [0, 1, 2]
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

print('ok')
