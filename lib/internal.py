# Set up some self-hosted methods

def listEqual(a, b):
    if len(a) != len(b):
        return False
    for i in range(0, len(a)):
        if a[i] != b[i]:
            return False
    return True

def listNotEqual(a, b):
    return not listEqual(a, b)

list.__eq__ = listEqual
list.__ne__ = listNotEqual
tuple.__eq__ = listEqual
tuple.__ne__ = listNotEqual
