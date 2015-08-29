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
    assert(next(i) == 1)
    assert(next(i) == 2)
    assert(next(i) == 3)
    caught = False
    try:
        next(i)
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

assert(list(map(inc, [])) == [])
assert(list(map(inc, [1, 2, 3])) == [2, 3, 4])

assert(list(zip()) == [])
assert(list(zip([1], [2])) == [(1, 2)])
assert(list(zip([1, 2], [2, 3])) == [(1, 2), (2, 3)])

assert(sum([]) == 0)
assert(sum([1, 2, 3]) == 6)
assert(sum((1, 2, 3), 4) == 10)

# todo: these tests are probably not sufficient
assert(divmod(5, 2) == (2, 1))
assert(divmod(4.5, 1.5) == (3.0, 0.0))
assert(divmod(5, 1.5) == (3.0, 0.5))
assert(divmod(1, -1) == (-1, 0))
assert(divmod(-1, 1) == (-1, 0))

print('ok')
