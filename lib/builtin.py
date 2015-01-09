# Some self-hosted builtins

def len(x):
    return x.__len__()

def str(x):
    return x.__str__()

def repr(x):
    return x.__repr__()

def print(x):
    str(x)._print()

def range(start, stop):
    # todo: check arguments are integers
    result = []
    i = start
    while i < stop:
        result.append(i)
        i = i + 1
    return result
