# output: ok

# 'type' not fully implemented
typ = object.__class__

assert object.__class__ == typ
assert object.__bases__ == ()
assert object.__mro__ == (object,)
assert typ.__class__ == typ
assert typ.__bases__ == (object,)
assert typ.__mro__ == (typ, object)

class C1:
  pass

c1 = C1()
c1.instance = 1
assert c1.instance == 1

class C2:
  static = 1
assert C2.static == 1
c2 = C2()
assert c2.static == 1
c2.static = 2
assert c2.static == 2
c2 = C2()
assert c2.static == 1

class C3:
  def set(self, x):
    self.x = x

  def get(self):
    return self.x

c3 = C3()
c3.set(3)
assert c3.get() == 3

class C4(C3):
  def get2(self):
    return self.x + 2

c4 = C4()
c4.set(3)
assert c4.get2() == 5

assert isinstance(c1, C1)
assert isinstance(c2, C2)
assert not isinstance(c1, C2)
assert not isinstance(c2, C1)
assert isinstance(c3, C3)
assert not isinstance(c3, C4)
assert isinstance(c4, C3)
assert isinstance(c4, C4)

class A:
  attr = 1

class B(A):
  pass

assert(B().attr == 1)
assert(B.attr == 1)

class C4:
  def f():
    pass

assert C4.__name__ == "C4"
assert C4.__bases__ == (object,)

c = C4()
assert type(c).__name__ == "C4"

def g():
  pass

assert type(g).__name__ == "function"
assert type(C4.f).__name__ == "function"
assert type(c.f).__name__ == "method"

class C5:
  def __init__(self, value):
    self.value = value

  def foo(self):
    return self.value

c = C5(42)
f = c.foo
assert f() == 42

class D:
  def __init__(self, value):
    self.value = value

  def __call__(self):
    return self.value

d = D(7)
assert d() == 7

class C6:
  f = d

c = C6()
assert c.f() == 7

def g():
  return 8

c.f = g
assert c.f() == 8

class E:
  def f(self, a, b, c, d, e, f, g, h, i, j, k, l):
    return a + b + c + d + e + f + g + h + i + j + k + l

e = E()
assert(e.f(1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1) == 12)

class F:
  pass

f = F()
f.g = g
assert f.g() == 8

F.g = lambda x: 9
assert f.g() == 8

del f.g
assert f.g() == 9

f = F()
f.__contains__ = lambda x: True

assert f.__contains__(0)
threw = False
try:
  0 in f
except TypeError as e:
  threw = 'not iterable' in str(e)
assert threw

del f.__contains__
F.__contains__ = lambda x, y: True

assert 0 in f

class Callable:
  def __call__(self, x):
    return isinstance(self, Callable)

f = F()
F.__contains__ = Callable()

assert 0 in f

# Class attributes are modifiable after class defintion
class G: pass
G.foo = 1
assert G.foo == 1
del G.foo

# Test error thrown in constructor
class Bad:
  def __init__(self):
    raise OSError()

threw = False
try:
  Bad()
except OSError:
  threw = True
assert threw

# decorators
def d1(x):
    return 1
@d1
class H:
    pass
assert H == 1

def d2(y):
    return lambda x: y
@d2(2)
def I():
    pass
assert I == 2

print("ok")
