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

print('ok')
