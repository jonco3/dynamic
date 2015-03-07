# output: ok

a, b, c, d, e, f = 1000, 1000, 1000, 1000, 1000, 1000
g, h, i, j, k, l = 2, 1000, 1000, 1000, 1000, 1000

a = a + 1
b = b - 2
c = c * 3
d = d / 4
e = e // 5
f = f % 6
g = g ** 7
h = h << 8
i = i >> 9
j = j & 10
k = k ^ 11
l = l | 12

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
    def __add__(self, other):
        return Wrapped(self.value + other)
    def __sub__(self, other):
        return Wrapped(self.value - other)
    def __mul__(self, other):
        return Wrapped(self.value * other)
    def __div__(self, other):
        return Wrapped(self.value / other)
    def __floordiv__(self, other):
        return Wrapped(self.value // other)
    def __mod__(self, other):
        return Wrapped(self.value % other)
    def __pow__(self, other):
        return Wrapped(self.value ** other)
    def __lshift__(self, other):
        return Wrapped(self.value << other)
    def __rshift__(self, other):
        return Wrapped(self.value >> other)
    def __or__(self, other):
        return Wrapped(self.value | other)
    def __and__(self, other):
        return Wrapped(self.value & other)
    def __xor__(self, other):
        return Wrapped(self.value ^ other)
    def __radd__(self, other):
        return other + self.value
    def __rsub__(self, other):
        return other - self.value
    def __rmul__(self, other):
        return other * self.value
    def __rdiv__(self, other):
        return other / self.value
    def __rfloordiv__(self, other):
        return other // self.value
    def __rmod__(self, other):
        return other % self.value
    def __rpow__(self, other):
        return other ** self.value
    def __rlshift__(self, other):
        return other << self.value
    def __rrshift__(self, other):
        return other >> self.value
    def __ror__(self, other):
        return other | self.value
    def __rand__(self, other):
        return other & self.value
    def __rxor__(self, other):
        return other ^ self.value

a, b, c, d, e, f = Wrapped(1000), Wrapped(1000), Wrapped(1000), Wrapped(1000), Wrapped(1000), Wrapped(1000)
g, h, i, j, k, l = Wrapped(2), Wrapped(1000), Wrapped(1000), Wrapped(1000), Wrapped(1000), Wrapped(1000)

a = a + 1
b = b - 2
c = c * 3
d = d / 4
e = e // 5
f = f % 6
g = g ** 7
h = h << 8
i = i >> 9
j = j & 10
k = k ^ 11
l = l | 12

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

a, b, c, d, e, f = 1000, 1000, 1000, 1000, 1000, 1000
g, h, i, j, k, l = 2, 1000, 1000, 1000, 1000, 1000

a = a + Wrapped(1)
b = b - Wrapped(2)
c = c * Wrapped(3)
d = d / Wrapped(4)
e = e // Wrapped(5)
f = f % Wrapped(6)
g = g ** Wrapped(7)
h = h << Wrapped(8)
i = i >> Wrapped(9)
j = j & Wrapped(10)
k = k ^ Wrapped(11)
l = l | Wrapped(12)

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

print('ok')
