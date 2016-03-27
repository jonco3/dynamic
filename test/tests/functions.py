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

# default arguments
def r(x, y = []):
    y.append(x)
    return y
assert r(2, [1]) == [1, 2]
assert r(3) == [3]
assert r(4) == [3, 4]

# keyword arguments
def s(a = -1, b = -2 , c = -3):
    return a, b, c

assert s(1, 2, 3) == (1, 2, 3)
assert s(1, 2, c = 3) == (1, 2, 3)
assert s(1, c = 3, b = 2) == (1, 2, 3)
assert s(c = 1, b = 2, a = 3) == (3, 2, 1)
assert s(b = 2) == (-1, 2, -3)
assert s(1, c = 3) == (1, -2, 3)

def raisesException(thunk, exception):
    ok = False
    try:
        thunk()
    except exception:
        ok = True
    return ok

assert raisesException(lambda: eval("s(a = 1, a = 1)"), SyntaxError)
assert raisesException(lambda: s(1, a = 1), TypeError)

def t(a, b = 2, c = 3):
    pass

assert raisesException(lambda: t(), TypeError)

def u(*a, b = 2, c = 3):
    return a, b, c

def v(b = 2, c = 3, *a):
    return a, b, c

assert u() == ((), 2, 3)
assert u(0, 1) == ((0, 1), 2, 3)
assert v() == ((), 2, 3)
assert v(0, 1) == ((), 0, 1)

# argument list unpacking, or the splat operator

def w(a, b, c):
    return a, b, c

assert w(*[1, 2, 3]) == (1, 2, 3)

def x(*a):
    return a

assert x(*[1, 2, 3]) == (1, 2, 3)
assert x(1, *(2, 3), 4) == (1, 2, 3, 4)

# check stack is expanded correctly for unknown size arguments list
x(*range(100))

# decorators
def d1(x):
    return 1
@d1
def y():
    pass
assert y == 1

def d2(y):
    return lambda x: y
assert d2(0)(1) == 0

@d2(2)
def z():
    pass
assert z == 2

print('ok')
