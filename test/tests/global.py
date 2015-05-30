# output: ok

foo = 0

def e():
    return foo
assert e() == 0

def f():
    foo = 1
f()
assert foo == 0

def g():
    global foo
    foo = 1
g()
assert foo == 1

for i in [2]:
    foo = i
assert foo == 2

def h():
    global bar
    bar = 2
h()
assert bar == 2

def i():
    baz = 0
    def inner():
        baz = 1
    inner()
    assert baz == 0
i()

def j():
    baz = 0
    def inner():
        nonlocal baz
        baz = 1
    inner()
    assert baz == 1
j()

foo = 0
def k():
    foo = 0
    def inner():
        nonlocal foo
        foo = 1
    inner()
    assert foo == 1
k()
assert foo == 0

def l():
    foo = 0
    def inner():
        global foo
        foo = 1
    inner()
    assert foo == 0
l()
assert foo == 1

def m1():
    global baz
    baz = 1
def m2():
    return baz
def m3():
    global baz
    del baz
m1(); m2(); m3()

assert len != 1
def n():
    global len
    len = 1
n()
assert len == 1

len = 2

del len
assert len != 1

print('ok')

