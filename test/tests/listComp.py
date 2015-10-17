# output: ok

assert([x for x in ()] == [])
assert([x for x in range(0, 3)] == [0, 1, 2])
assert([(x, y) for x in range(0, 2) for y in range(2, 4)] ==
       [(0, 2), (0, 3), (1, 2), (1, 3)])
assert([x for x in range(0, 3) if x >= 1] == [1, 2])
print('ok')
