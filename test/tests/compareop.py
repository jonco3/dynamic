# output: ok

# __eq__

assert 0 == 0
assert not 0 == 1

assert 1.5 == 1.5
assert not 1.5 == 0.5

assert not 0.5 == 0
assert not 0 == 0.5

assert "foo" == "foo"
assert not "foo" == "bar"
assert not "foo" == 0
assert not 0 == "foo"
assert not "foo" == 0.5
assert not 0.5 == "foo"

# __ne__

assert not 0 != 0
assert 0 != 1

assert not 1.5 != 1.5
assert 1.5 != 0.5

assert 0.5 != 0
assert 0 != 0.5

assert not "foo" != "foo"
assert "foo" != "bar"
assert "foo" != 0
assert 0 != "foo"
assert "foo" != 0.5
assert 0.5 != "foo"

# __lt__

assert 0 < 1
assert not 1 < 1
assert not 2 < 1

assert 0.5 < 1.5
assert not 1.5 < 1.5
assert not 2.5 < 1.5

assert 0 < 0.5
assert 0.5 < 1

assert "bar" < "foo"
assert not "bar" < "bar"
assert not "baz" < "bar"

# __le__

assert 0 <= 1
assert 1 <= 1
assert not 2 < 1

assert 0.5 <= 1.5
assert 1.5 <= 1.5
assert not 2.5 < 1.5

assert 0 <= 0.5
assert 0.5 <= 1

assert "bar" <= "foo"
assert "bar" <= "bar"
assert not "baz" <= "bar"

# __gt__

assert 1 > 0
assert not 1 > 1
assert not 1 > 2

assert 1.5 > 0.5
assert not 1.5 > 1.5
assert not 1.5 > 2.5

assert 0.5 > 0
assert 1 > 0.5

assert "foo" > "bar"
assert not "bar" > "bar"
assert not "bar" > "baz"

# __ge__

assert 1 >= 0
assert 1 >= 1
assert not 1 >= 2

assert 1.5 >= 0.5
assert 1.5 >= 1.5
assert not 1.5 >= 2.5

assert 0.5 >= 0
assert 1 >= 0.5

assert "foo" >= "bar"
assert "bar" >= "bar"
assert not "bar" >= "baz"

def testCompare(x):
    assert x == 1
    assert not x == 0
    assert x != 0
    assert not x != 1
    assert x < 2
    assert not x < 1
    assert x <= 1
    assert x <= 2
    assert not x <= 0
    assert 1 == x
    assert not 0 == x
    assert 2 > x
    assert not 1 > x
    assert 1 >= x
    assert 2 >= x
    assert not 0 >= x

class X:
    def __init__(self, value):
        self.value = value
    def __eq__(self, other):
        return self.value == other
    def __lt__(self, other):
        return self.value < other
    def __le__(self, other):
        return self.value <= other

class Y(X):
    def __init__(self, value):
        self.value = value
    def __ne__(self, other):
        return self.value != other
    def __gt__(self, other):
        return self.value > other
    def __ge__(self, other):
        return self.value >= other

class Z:
    def __init__(self, value):
        self.value = value
    def __ne__(self, other):
        return self.value != other
    def __gt__(self, other):
        return self.value > other
    def __ge__(self, other):
        return self.value >= other

testCompare(X(1))
testCompare(Y(1))

z = Z(1)
assert z != 0
assert not z == 1 # default __eq__ is object identity
assert z > 0
assert z >= 1

exception = False
try:
    z < 1
except TypeError as e:
    exception = True
assert exception

exception = False
try:
    z <= 1
except TypeError as e:
    exception = True
assert exception

print('ok')
