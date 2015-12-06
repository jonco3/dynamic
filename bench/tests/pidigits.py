# args: 100
# output: 3141592653	:10
# output: 5897932384	:20
# output: 6264338327	:30
# output: 9502884197	:40
# output: 1693993751	:50
# output: 582097494	:60
# output: 4592307816	:70
# output: 4062862089	:80
# output: 9862803482	:90
# output: 5342117067	:100
# bench-args: 4000

# The Computer Language Benchmarks Game
# http://benchmarksgame.alioth.debian.org/
# contributed by Joseph LaFata



# COMMAND LINE:
# /usr/local/src/Python-3.4.0/bin/python3.4 pidigits.py 10000

import sys

try:
    N = int(sys.argv[1])
except:
    N = 100

i = k = ns = 0
k1 = 1
n,a,d,t,u = (1,0,1,0,0)
while(1):
    k += 1
    t = n<<1
    n *= k
    a += t
    k1 += 2
    a *= k1
    d *= k1
    if a >= n:
        t,u = divmod(n*3 +a,d)
        u += n
        if d > u:
            ns = ns*10 + t
            i += 1
            if i % 10 == 0:
                print(str(ns) + '\t:' + str(i))
                ns = 0
            if i >= N:
                break
            a -= d*t
            a *= 10
            n *= 10
