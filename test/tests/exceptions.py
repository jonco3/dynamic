# output: ok

class TestError(RuntimeError):
    def __init__(self, message):
        self.message = message

# Catch exception
caught = False
try:
    raise TestError("foo")
except:
    caught = True
assert caught

# Catch exception via exact class
caught = False
try:
    raise TestError("foo")
except TestError as e:
    caught = True
    assert e.message == "foo"
assert caught

# Catch exception via superclass
caught = False
try:
    raise TestError("foo")
except RuntimeError as e:
    caught = True
assert caught

# Catch exception with multiple except blocks
caught = False
try:
    raise TestError("foo")
except NameError as e:
    pass
except TestError as e:
    caught = True
assert caught

# Catch exception with multiple except blocks
caught = False
try:
    raise TestError("foo")
except RuntimeError as e:
    caught = True
except TestError as e:
    pass
assert caught

# Catch exception with multiple except blocks
caught = False
try:
    raise TestError("foo")
except TestError as e:
    caught = True
except:
    pass
assert caught

# Catch exception with multiple except blocks
caught = False
try:
    raise TestError("foo")
except NameError as e:
    pass
except:
    caught = True
assert caught

# Catch exception from called function
def a():
    raise TestError('foo')

caught = False
try:
    a()
except TestError as e:
    caught = True
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
    except TestError as e:
        caught = True
        assert caught

b1()

# Finally
finals = 0
def c2():
    global finals
    try:
        raise TestError('foo')
    finally:
        finals |= 1

def c1():
    global finals
    try:
        c2()
    finally:
        finals |= 2

try:
    c1()
except TestError:
    pass
assert finals == 3

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
except exception1 as e:
    caught = True
except exception2 as e:
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
except TestError as e:
    caught = True
assert caught
assert finals == 1

# else clause runs at the end
elseRan = False
try:
    pass
except:
    pass
else:
    elseRan = True
assert elseRan

# Binding in except block deletes shadowed name?
foo = 0
try:
    raise Exception()
except Exception as foo:
    pass

caught = False
try:
    foo
except NameError as e:
    caught = True
assert caught

print('ok')
