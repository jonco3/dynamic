# output: ok

assert(max(1, 2, 3) == 3)
assert(max(1, 3, 2) == 3)
assert(max(3, 2, 1) == 3)
assert(min([1]) == 1)
assert(min([1, 2, 3]) == 1)
assert(min([1, 3, 2]) == 1)
assert(min([3, 2, 1]) == 1)

exception = False
try:
    min()
except TypeError:
    exception = True
assert(exception)

exception = False
try:
    max()
except TypeError:
    exception = True
assert(exception)

def testIter(iterable):
    i = iter(iterable)
    assert(i.next() == 1)
    assert(i.next() == 2)
    assert(i.next() == 3)
    caught = False
    try:
        i.next()
    except StopIteration:
        caught = True
    assert(caught)

class OwnSequence:
    def __init__(self, wrapped):
        self.wrapped = wrapped

    def __getitem__(self, index):
        return self.wrapped[index]


testIter([1, 2, 3])
testIter(OwnSequence([1, 2, 3]))

def inc(x):
    return x + 1

assert(map(inc, []) == [])
assert(map(inc, [1, 2, 3]) == [2, 3, 4])

assert(list(zip()) == [])
assert(list(zip([1], [2])) == [(1, 2)])
assert(list(zip([1, 2], [2, 3])) == [(1, 2), (2, 3)])

print('ok')
