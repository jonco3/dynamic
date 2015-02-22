# output: ok

foo = 0

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

print('ok')

