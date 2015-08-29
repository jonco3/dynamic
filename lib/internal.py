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
