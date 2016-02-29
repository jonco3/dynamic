# output: ok

# Check enough stack space is reserved for handling default arguments

def rr(a = 1, b = 2, c = 3):
    return 5

def rrr(a = 1, b = 2, c = 3):
    return rr()

assert rrr() == 5

print("ok")
