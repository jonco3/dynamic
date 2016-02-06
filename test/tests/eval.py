# output: ok

assert eval("1") == 1
assert eval("2 + 2") == 4

g = 1
r = eval("g")
assert r == 1

def readGlobal():
    return eval("g")
assert readGlobal() == 1

def readNonLocal():
    x = 4
    def f():
        return eval("x")
    return f
failed = False
try:
    readNonLocal()()
except NameError:
    failed = True
assert failed

def readLocal():
    x = 5
    return eval("x")
assert readLocal() == 5

globalDict = {'x': 6}
localDict = {'x': 7}
assert eval("x", globalDict) == 6
assert eval("x", None, localDict) == 7
assert eval("x", globalDict, localDict) == 7

print("ok")
