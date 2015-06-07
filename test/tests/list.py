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

print('ok')
