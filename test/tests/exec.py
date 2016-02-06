# output: ok

assert exec("1") == None

result = None
def setResult(x):
    global result
    result = x

# Read global
g = 1
exec("setResult(g)")
assert result == 1

def readGlobal():
    exec("setResult(g)")
result = None
readGlobal()
assert result == 1

# Write global
exec("g = 2")
assert g == 2

def writeGlobal():
    return exec("global g; g = 3")
writeGlobal()
assert g == 3

# Can't access nonlocals
def readNonLocal():
    x = 4
    def f():
        return exec("setResult(x)")
    return f
failed = False
try:
    readNonLocal()()
except NameError:
    failed = True
assert failed

# Read local
def readLocal():
    x = 5
    return exec("setResult(x)")
result = None
readLocal()
assert result == 5

# Can't write locals
def writeLocal():
    x = 6
    exec("x = 7")
    return x
assert writeLocal() == 6
assert 'x' not in globals().keys()

globalDict = {'x': 6, 'setResult': setResult}
localDict = {'x': 7}

result = None
exec("setResult(x)", globalDict)
assert result == 6

result = None
exec("setResult(x)", None, localDict)
assert result == 7

result = None
exec("setResult(x)", globalDict, localDict)
assert result == 7

exec("global x; x = 8", globalDict)
assert globalDict['x'] == 8

exec("global x; del x", globalDict)
assert 'x' not in globalDict

exec("x = 9", None, localDict)
assert localDict['x'] == 9

globalDict['x'] = 8
exec("x = 10", globalDict, localDict)
assert globalDict['x'] == 8
assert localDict['x'] == 10

exec("global x; x = 11", globalDict, localDict)
assert globalDict['x'] == 11
assert localDict['x'] == 10

exec("y = 12", globalDict, localDict)
assert 'y' not in globalDict
assert localDict['y'] == 12

exec("global z; z = 13", globalDict, localDict)
assert globalDict['z'] == 13

aGlobal = None
dict = {}
exec("", dict)
assert "aGlobal" not in dict
assert "__builtins__" in dict

print("ok")
