# Some self-hosted builtins that are visible in the global namespace

def len(x):
    return x.__len__()

def repr(x):
    return x.__repr__()

def dump(x):
    x.__dump__()

def print(*args):
    s = ""
    for x in args:
        if (len(s) != 0):
            s += " "
        s += str(x)
    s._print()

def type(x):
    return x.__class__

def range(*args):
    if len(args) == 1:
        start, end, step = 0, args[0], 1
    elif len(args) == 2:
        start, end, step = args[0], args[1], 1
    elif len(args) == 3:
        start, end, step = args
    else:
        raise TypeError("Wrong number of arguments to range")
    # todo: check arguments are integers
    # todo: should be an iterator
    result = []
    i = start
    while i < end:
        result.append(i)
        i += step
    return result

def max(*args):
    l = len(args)
    if l == 0:
        raise TypeError("Not enough arguments to max")
    elif l == 1:
        args = args[0]
    r = None
    for x in args:
        if r is None or x > r:
            r = x
    return r

def min(*args):
    l = len(args)
    if l == 0:
        raise TypeError("Not enough arguments to max")
    elif l == 1:
        args = args[0]
    r = None
    for x in args:
        if r is None or x < r:
            r = x
    return r

def next(iterator):
    # todo: default arg
    return iterator.__next__()

def zip(*iterables):
    if len(iterables) == 0:
        return
    iters = list(map(iter, iterables))
    try:
        while True:
            elements = []
            for i in iters:
                elements.append(next(i))
            yield tuple(elements)
    except StopIteration:
        pass

def map(function, *iterables):
    if len(iterables) == 0:
        raise TypeError("Not enough arguments to map")
    elif len(iterables) == 1:
        for x in iterables[0]:
            yield(function(x))
    else:
        for x in zip(iterables):
            raise Exception("not implmemented")
            #yield(function(*x))

def sum(iterable, start = 0):
    result = start
    for element in iterable:
        result += element
    return result

def divmod(a, b):
    return a // b, a % b

def sorted(l):
    copy = l[:]
    copy.sort()
    return copy
