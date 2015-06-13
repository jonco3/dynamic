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

getArgs = None

class NonDataDescriptor:
  def __get__(self, instance, owner):
    global getArgs
    getArgs = (self, instance, owner)
    return 0

obj = C()
desc = NonDataDescriptor()
obj.attr = desc
assert obj.attr == desc
assert getArgs == None
del obj.attr

C.attr = desc
assert C.attr == 0
assert getArgs == (desc, None, C)
assert obj.attr == 0
assert getArgs == (desc, obj, C)
obj.attr = 1
assert obj.attr == 1
del C.attr

getArgs = None
setArgs = None
delArgs = None

class DataDescriptor:
  def __init__(self):
    self.value = 0

  def __get__(self, instance, owner):
    global getArgs
    getArgs = (self, instance, owner)
    return self.value

  def __set__(self, instance, value):
    global setArgs
    setArgs = (self, instance, value)
    self.value = value

  def __delete__(self, instance):
    global delArgs
    delArgs = (self, instance)
    self.value = 0

obj = C()
desc = DataDescriptor()
obj.attr = desc
assert obj.attr == desc
del obj.attr

C.attr = desc
assert C.attr == 0
assert getArgs == (desc, None, C)
assert obj.attr == 0
assert getArgs == (desc, obj, C)
obj.attr = 1
assert obj.attr == 1
assert setArgs == (desc, obj, 1)
del obj.attr
assert obj.attr == 0
assert delArgs == (desc, obj)
del C.attr

print('ok')
