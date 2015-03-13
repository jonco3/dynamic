# output: 55

def fib():
    a, b = 0, 1
    while 1:
        yield a
        a, b = b, a + b

f = fib()
for i in range(10):
    f.next()

print(f.next())
