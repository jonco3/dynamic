# Some self-hosted builtins that are visible in the global namespace

def len(x):
    return x.__len__()

def str(x):
    return x.__str__()

def repr(x):
    if hasattr(x, "__repr__"):
        return x.__repr__()
    else:
        return str(x)

def dump(x):
    if hasattr(x, "__dump__"):
        x.__dump__()
    else:
        print(repr(x))

def print(x):
    str(x)._print()

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
