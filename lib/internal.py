# Set up some self-hosted methods

def sequenceEqual(a, b):
    if len(a) != len(b):
        return False
    for i in range(0, len(a)):
        if a[i] != b[i]:
            return False
    return True

def listEqual(a, b):
    if not isinstance(b, list):
        return False
    return sequenceEqual(a, b)

def listNotEqual(a, b):
    return not listEqual(a, b)

def tupleEqual(a, b):
    if not isinstance(b, tuple):
        return False
    return sequenceEqual(a, b)

def tupleNotEqual(a, b):
    return not tupleEqual(a, b)

def dictEqual(a, b):
    if not isinstance(b, dict):
        return False
    if len(a) != len(b):
        return False
    for key in a.keys():
        if key not in b or a[key] != b[key]:
            return False
    return True

def dictNotEqual(a, b):
    return not dictEqual(a, b)

list.__eq__ = listEqual
list.__ne__ = listNotEqual
tuple.__eq__ = tupleEqual
tuple.__ne__ = tupleNotEqual
dict.__eq__ = dictEqual
dict.__ne__ = dictNotEqual

def listExtend(a, b):
    for element in b:
        a.append(element)

list.extend = listExtend

def strJoin(self, seq):
    if not isinstance(self, str):
        raise TypeError("Expecting string")

    result = ""
    first = True
    for s in seq:
        if not isinstance(s, str):
            raise TypeError("Expecting string")
        if not first:
            result += self
        result += s
        first = False

    return result

str.join = strJoin

class SequenceIterator:
    def __init__(self, target):
        self.target = target
        self.index = 0

    def __next__(self):
        try:
            result = self.target[self.index]
            self.index += 1
            return result
        except IndexError:
            raise StopIteration()

def iterableToList(iterable):
    result = []
    for x in iterable:
        result.append(x)
    return result

def inUsingIteration(iterable, object):
    for x in iterable:
        if x == object:
            return True
    return False

def inUsingSubscript(container, object):
    i = 0
    try:
        while True:
            if container[i] == object:
                return True
            i += 1
    except IndexError:
        return False

def __import__(name, globals = None, locals = None, fromList = (), level = 0):
    return sys.modules[name]
