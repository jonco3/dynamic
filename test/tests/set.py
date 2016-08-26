# output: ok

a = set()
assert(len(a) == 0)
assert(1 not in a)

a.add(1)
assert(len(a) == 1)
assert(1 in a)
assert(2 not in a)

a.add(2)
a.add(3)
assert(len(a) == 3)
assert(1 in a)
assert(2 in a)
assert(3 in a)
assert(4 not in a)

assert(len({1, 2, 3}) == 3)
assert(a == {3, 2, 1})
assert(set() != {1})

print('ok')
