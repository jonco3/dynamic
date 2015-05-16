# output: ok

# delete attribute
class C:
    def __init__(self):
        self.a = 1
x = C()
assert x.a == 1
del x.a
found = True
try:
    x.a
except AttributeError:
    found = False
assert not found

# delete subscript
x = {'a': 1, 'b': 2}
del x['a']
assert x == {'b': 2}

# delete subscript
x = [1, 2, 3]
del x[1]
assert x == [1, 3]

# delete slice
#x = [1, 2, 3]
#del x[1:2]
#assert x == [1, 3]

# delete global name
a = 1
del a
found = True
try:
    a
except NameError:
    found = False
assert not found

# delete local name
def f():
    a = 1
    del a
    found = True
    try:
        a
    except NameError:
        found = False
    assert not found
f()

# delete lexical name
def g():
    a = 1
    def inner():
        nonlocal a
        del a
        found = True
        try:
            a
        except NameError:
            found = False
        assert not found
    inner()
g()

print('ok')
