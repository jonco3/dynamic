# output: ok

a = {}
assert(len(a) == 0)

for i in range(0, 100):
    a[i] = i * 2

assert(len(a) == 100)
for i in range(0, 100):
    assert i in a
    assert(a[i] == i * 2)
assert 101 not in a

for i in range(0, 100, 2):
    del a[i]
assert(len(a) == 50)
for i in range(0, 100):
    assert (i in a) == ((i % 2) != 0)

a = {}
for i in range(0, 100):
    a[str(i)] = i
assert(len(a) == 100)
for i in range(0, 100):
    k = str(i)
    assert k in a
    assert(a[k] == i)
assert '101' not in a

for i in range(0, 100, 2):
    del a[str(i)]
assert(len(a) == 50)
for i in range(0, 100):
    assert (str(i) in a) == ((i % 2) != 0)

print('ok')
