# output: ok

assert(list() == [])
assert(list([1, 2, 3]) == [1, 2, 3])
assert(list((1, 2, 3)) == [1, 2, 3])

class ListDerived(list):
    pass

assert(ListDerived() == [])
assert(ListDerived([1, 2, 3]) == [1, 2, 3])
assert(ListDerived((1, 2, 3)) == [1, 2, 3])

assert(tuple() == ())
assert(tuple([1, 2, 3]) == (1, 2, 3))
assert(tuple((1, 2, 3)) == (1, 2, 3))

class TupleDerived(tuple):
    pass

assert(TupleDerived() == ())
assert(TupleDerived([1, 2, 3]) == (1, 2, 3))
assert(TupleDerived((1, 2, 3)) == (1, 2, 3))

def empty():
    if False:
        yield 1

def gen():
    yield 1
    yield 2
    yield 3

assert(list(empty()) == [])
assert(tuple(empty()) == ())
assert(list(gen()) == [1, 2, 3])
assert(tuple(gen()) == (1, 2, 3))

a = [1, 2]
a.extend([3, 4])
assert(a == [1, 2, 3, 4])
a = [1, 2]
a.extend((3, 4))
assert(a == [1, 2, 3, 4])

assert([] == [])
assert(() == ())
assert(() != [])
assert([] != ())

a = [3, 1, 2]
assert(sorted(a) == [1, 2, 3])
assert(a == [3, 1, 2])
a.sort()
assert(a == [1, 2, 3])

assert [] + [] == []
assert [1] + [] == [1]
assert [] + [2] == [2]
assert [1, 2] + [3, 4] == [1, 2, 3, 4]
assert () + () == ()
assert (1,) + () == (1,)
assert () + (2,) == (2,)
assert (1, 2) + (3, 4) == (1, 2, 3, 4)

assert [1, 2] * 0 == []
assert [1, 2] * 2 == [1, 2, 1, 2]
assert (1, 2) * 0 == ()
assert (1, 2) * 2 == (1, 2, 1, 2)

r = []
for x, y in enumerate(['a', 'b']):
    r.append(x)
    r.append(y)
assert r == [0, 'a', 1, 'b']

print('ok')
