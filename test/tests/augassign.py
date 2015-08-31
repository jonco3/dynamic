# output: ok

v = [10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10]
r = [11, 8, 30, 2.5, 2, 4, 10000000, 20, 2, 2, 14, 15]

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

assert v == r

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

v = list(map(Wrapped, [10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10]))

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

assert list(map(lambda x: x.value, v)) == r

print('ok')
