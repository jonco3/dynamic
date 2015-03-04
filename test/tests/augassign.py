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
