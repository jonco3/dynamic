# output: ok

assert(max(1, 2, 3) == 3)
assert(max(1, 3, 2) == 3)
assert(max(3, 2, 1) == 3)
assert(min([1]) == 1)
assert(min([1, 2, 3]) == 1)
assert(min([1, 3, 2]) == 1)
assert(min([3, 2, 1]) == 1)

print('ok')
