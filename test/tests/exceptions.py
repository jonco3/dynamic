# output: ok

class TestError(RuntimeError):
    def __init__(self, message):
        self.message = message

# Catch exception via exact class
caught = False
try:
    raise TestError("foo")
except TestError, e:
    caught = True
    assert e.message == "foo"
assert caught

# Catch exception via superclass
caught = False
try:
    raise TestError("foo")
except RuntimeError, e:
    caught = True
    assert e.message == "foo"
assert caught

# Catch exception with multiple except blocks
caught = False
try:
    raise TestError("foo")
except NameError, e:
    pass
except TestError, e:
    caught = True
    assert e.message == "foo"
assert caught

# Catch exception with multiple except blocks
caught = False
try:
    raise TestError("foo")
except RuntimeError, e:
    caught = True
    assert e.message == "foo"
except TestError, e:
    pass
assert caught

# Catch exception from called function
def a():
    raise TestError('foo')

caught = False
try:
    a()
except TestError, e:
    caught = True
    assert e.message == "foo"
assert caught

# Catch exception from called function
def b2():
    try:
        raise TestError('foo')
    except NameError:
        assert False

def b1():
    caught = False
    try:
        b2()
    except TestError, e:
        caught = True
        assert e.message == "foo"
    assert caught

b1()

# Finally
finals = 0
def c2():
    global finals
    try:
        raise TestError('foo')
    finally:
        finals += 1

def c1():
    global finals
    try:
        c2()
    finally:
        finals += 1

try:
    c1()
except TestError:
    pass
assert finals == 2

# finally can clobber return value
def foo():
    try:
        return 'try'
    finally:
        return 'finally'
assert foo() == "finally"

# except expr gets evaluated after the try block
caught = False
exception1 = NameError
exception2 = TestError
try:
    exception1, exception2 = exception2, exception1
    raise TestError("foo")
except exception1, e:
    caught = True
    assert e.message == "foo"
except exception2, e:
    assert False
assert caught

# exception raised in except clause
def d():
    raise TestError("foo")
    return None

caught = False
finals = 0
try:
    try:
        raise NameError()
    except d():
        assert False
    finally:
        finals += 1
except TestError, e:
    caught = True
    assert e.message == "foo"
assert caught
assert finals == 1

print('ok')
