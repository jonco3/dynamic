# output: ok

assert([x for x in ()] == [])
assert([x for x in range(0, 3)] == [0, 1, 2])
assert([(x, y) for x in range(0, 2) for y in range(2, 4)] ==
       [(0, 2), (0, 3), (1, 2), (1, 3)])
assert([x for x in range(0, 3) if x >= 1] == [1, 2])

def inc(x):
    return x + 1
assert([inc(y) for y in (1, 2, 3)] == [2, 3, 4])

a = 1
assert([a for y in (1, 2, 3)] == [1, 1, 1])

assert([(lambda x: x * 2)(y) for y in (1, 2, 3)] == [2, 4, 6])
assert([(lambda x: y * 2)(y) for y in (1, 2, 3)] == [2, 4, 6])

print('ok')
