# output: ok

def f(x):
    return x + 1
assert f(1) == 2

def g(x):
    return f(x)
assert g(2) == 3

def h(x):
    def h2(y):
        return y + 2
    return h2(x)
assert h(3) == 5

def i(x):
    def i(y):
        return y + 3
    return i(x)
assert i(3) == 6

def j(x):
    def j2(x):
        return x + 4
    return j2(x)
assert j(3) == 7

def k(x):
    if x > 10:
        return x
    else:
        return k2(x)
def k2(x):
    return k(x + 1)
assert k(1) == 11

def l(a, *b):
    return (a, b)
assert l(0) == (0, ())
assert l(1, 2) == (1, (2,))
assert l(1, 2, 3) == (1, (2, 3))

def m(a, b = 1, *c):
    return (a, b, c)
assert m(0) == (0, 1, ())
assert m(1, 2) == (1, 2, ())
assert m(1, 2, 3) == (1, 2, (3,))
assert m(1, 2, 3, 4) == (1, 2, (3, 4))

def n(a, b = 1, c = 2, *d):
    return (a, b, c, d)
assert n(0) == (0, 1, 2, ())
assert n(1, 2) == (1, 2, 2, ())
assert n(1, 2, 3) == (1, 2, 3, ())
assert n(1, 2, 3, 4) == (1, 2, 3, (4,))

def o(a, b, c, d, e, f, g, h, i, j, k, l):
    return a + b + c + d + e + f + g + h + i + j + k + l
assert(o(1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1) == 12)

# test lexical scoping

def p(x):
    def p2():
        return x
    return p2
assert(p(1)() == 1)

def q(x):
    foo = 0
    def q2():
        nonlocal foo
        foo = x
    q2()
    return foo
assert(q(1) == 1)

print('ok')
