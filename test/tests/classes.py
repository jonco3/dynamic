# output: ok

class C1:
  pass

c1 = C1()
c1.instance = 1
assert c1.instance == 1

class C2:
  static = 1
assert C2.static == 1

class C3:
  def set(self, x):
    self.x = 1

  def get(self):
    return self.x

c3 = C3()
c3.set(3)
assert c3.get() == 3

print("ok")
