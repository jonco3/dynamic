# output: ok

class C:
  pass

a = C()
a.foo = 1
assert a.foo == 1
a.bar = 2
assert a.foo == 1
assert a.bar == 2
a.foo = 3
assert a.foo == 3
assert a.bar == 2
print('ok')
