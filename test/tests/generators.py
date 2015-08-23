# output: ok

def foo(x):
    yield 1
    yield x

iter = foo(2)
assert next(iter) == 1
assert next(iter) == 2

def bar(array):
    for x in array:
        yield 2 * x

iter = bar([1, 2, 3])
assert next(iter) == 2
assert next(iter) == 4
assert next(iter) == 6

def collect(iter):
    result = []
    for x in iter:
        result.append(x)
    return result

r = collect(foo(0))
assert len(r) == 2
assert r[0] == 1
assert r[1] == 0

def noExcept(f):
    try:
        yield f()
    except:
        yield None

def a():
    return 1

def b():
    raise Exception()

assert(collect(noExcept(a)) == [1])
assert(collect(noExcept(b)) == [None])

print('ok')
