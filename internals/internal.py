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

def pathJoin(*path):
    # todo: obvs needs os.path.join here
    return '/'.join(path)

def loadModuleFile(name):
    modulePath = name.split(".")
    for dirpath in sys.path:
        try:
            packagePath = modulePath
            filename = pathJoin(dirpath, *packagePath, "__init__.py")
            f = open(filename)
        except OSError:
            try:
                packagePath = modulePath[:-1]
                filename = pathJoin(dirpath, *packagePath,
                                    modulePath[-1] + ".py")
                f = open(filename)
            except OSError:
                continue

        source = f.read()
        f.close()
        package = ".".join(packagePath)
        return package, source, filename

    return None, None, None

def loadModule(name):
    if name in sys.modules:
        return sys.modules[name]

    package, source, filename = loadModuleFile(name)
    if not source:
        return None

    module = execModule(name, package, source, filename)
    sys.modules[name] = module
    return module

def __import__(name, globals = None, locals = None, fromList = (), level = 0):
    if not isinstance(name, str):
        raise ValueError("Bad module name: " + str(name))

    names = name.split(".")

    if name not in sys.modules:
        # Load ancestors up to requested module
        path = []
        for name in names:
            path.append(name)
            module = loadModule(".".join(path))
            if not module:
                raise ImportError("Module not found: " + ".".join(path))

    return sys.modules[name] if fromList else sys.modules[names[0]]
