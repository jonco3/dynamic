# output: ok

input = [10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10]
expected = [11, 8, 30, 2.5, 2, 4, 10000000, 20, 2, 2, 14, 15]

def test(v):
    v[0] += 1
    v[1] -= 2
    v[2] *= 3
    v[3] /= 4
    v[4] //= 5
    v[5] %= 6
    v[6] **= 7
    v[7] <<= 1
    v[8] >>= 2
    v[9] &= 3
    v[10] ^= 4
    v[11] |= 5
    return v

assert test(list(input)) == expected

class Wrapped:
    def __init__(self, initial):
        self.value = initial
    def __eq__(self, other):
        return self.value == other
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

assert test(list(map(Wrapped, input))) == expected

class Wrapped2:
    def __init__(self, initial):
        self.value = initial
    def __add__(self, other):
        return Wrapped(self.value + other)
    def __sub__(self, other):
        return Wrapped(self.value - other)
    def __mul__(self, other):
        return Wrapped(self.value * other)
    def __truediv__(self, other):
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

assert test(list(map(Wrapped2, input))) == expected

class C:
    def __init__(self, value):
        self.value = value

o = C(1)

def incValue(self, other):
    self.value += other
    return self

o.__iadd__ = incValue

threw = False
try:
    o += 1
except TypeError as e:
    if "unsupported operand type" in str(e):
        threw = True
assert threw

C.__iadd__ = incValue
o += 1
assert o.value == 2

class NonDataDescriptor:
    def __get__(self, instance, owner):
        def f(other):
            o.value -= other
            return o
        return f

C.__iadd__ = NonDataDescriptor()
o += 1
assert o.value == 1

class D:
    def __init__(self, initial):
        self.value = initial
    def __iadd__(self, other):
        self.value += other
        return self
    def __add__(self, other):
        self.value -= other
        return self

class E:
    def __init__(self, initial):
        self.value = initial
    def __iadd__(self, other):
        self.value += other
        return self
    def __add__(self, other):
        return NotImplemented

class F:
    def __init__(self, initial):
        self.value = initial
    def __iadd__(self, other):
        return NotImplemented
    def __add__(self, other):
        self.value -= other
        return self

class G:
    def __init__(self, initial):
        self.value = initial
    def __iadd__(self, other):
        return NotImplemented
    def __add__(self, other):
        return NotImplemented

d = D(0); d += 1; assert d.value == 1
e = E(0); e += 1; assert e.value == 1
f = F(0); f += 1; assert f.value == -1
g = G(0);
threw = False
try:
    g += 1
except TypeError:
    threw  = True
assert threw
assert g.value == 0

print('ok')
