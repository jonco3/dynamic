# args: 10
# output: 55
# bench-args: 30
# bench-output: 832040

import sys

def fib(n):
    if n == 0:
        return 0
    elif n == 1:
        return 1
    else:
        return fib(n-1) + fib(n-2)

print(fib(int(sys.argv[1])))
