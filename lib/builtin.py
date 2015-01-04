# Some self-hosted builtins

def len(x):
    return x.__len__()

def str(x):
    return x.__str__()

def repr(x):
    return x.__repr__()

def print(x):
    str(x)._print()
