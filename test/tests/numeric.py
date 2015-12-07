# output: ok

assert 2 + 2 == 4
assert 3 * True == 3
assert False - 1.5 == -1.5

assert str(1) == '1'
assert str(True) == 'True'

assert 0x10 == 16
assert 4000000000 * 1000 == 4000000000000

assert 0 ** 0 == 1
assert 2 ** 0 == 1
assert 16 ** -1 == 0.0625
assert 2 ** 100 == 1267650600228229401496703205376

print('ok')
