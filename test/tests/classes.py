# output: ok

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
  pass

assert C4.__name__ == "C4"

print("ok")
