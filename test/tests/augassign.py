# output: ok

a, b, c, d, e, f = 1000, 1000, 1000, 1000, 1000, 1000
g, h, i, j, k, l = 2, 1000, 1000, 1000, 1000, 1000

a += 1
b -= 2
c *= 3
d /= 4
e //= 5
f %= 6
g **= 7
h <<= 8
i >>= 9
j &= 10
k ^= 11
l |= 12

assert a == 1001
assert b == 998
assert c == 3000
assert d == 250
assert e == 200
assert f == 4
assert g == 128
assert h == 256000
assert i == 1
assert j == 8
assert k == 995
assert l == 1004

class Wrapped:
    def __init__(self, initial):
        self.value = initial
    def __iadd__(self, other):
        self.value += other
        return self
    def __isub__(self, other):
        self.value -= other
        return self
    def __imul__(self, other):
        self.value *= other
        return self
    def __itruediv__(self, other):
        self.value /= other
        return self
    def __ifloordiv__(self, other):
        self.value //= other
        return self
    def __imod__(self, other):
        self.value %= other
        return self
    def __ipow__(self, other):
        self.value **= other
        return self
    def __ilshift__(self, other):
        self.value <<= other
        return self
    def __irshift__(self, other):
        self.value >>= other
        return self
    def __ior__(self, other):
        self.value |= other
        return self
    def __iand__(self, other):
        self.value &= other
        return self
    def __ixor__(self, other):
        self.value ^= other
        return self

a, b, c, d, e, f = Wrapped(1000), Wrapped(1000), Wrapped(1000), Wrapped(1000), Wrapped(1000), Wrapped(1000)
g, h, i, j, k, l = Wrapped(2), Wrapped(1000), Wrapped(1000), Wrapped(1000), Wrapped(1000), Wrapped(1000)

a += 1
b -= 2
c *= 3
d /= 4
e //= 5
f %= 6
g **= 7
h <<= 8
i >>= 9
j &= 10
k ^= 11
l |= 12

assert a.value == 1001
assert b.value == 998
assert c.value == 3000
assert d.value == 250
assert e.value == 200
assert f.value == 4
assert g.value == 128
assert h.value == 256000
assert i.value == 1
assert j.value == 8
assert k.value == 995
assert l.value == 1004

print('ok')
