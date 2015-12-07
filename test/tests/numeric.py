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

a = 8000000000
assert a ** 0 == 1
assert a ** -1 == 1.25e-10
assert a ** 2 == 64000000000000000000

assert 1 << 31 == 2147483648
assert 1 << 32 == 4294967296
assert 1 << 100 == 1267650600228229401496703205376
assert a << 1 == 16000000000

def raisesException(thunk):
    result = False
    try:
        thunk()
    except:
        result = True
    return result

assert raisesException(lambda: 1 << -1)
assert raisesException(lambda: 1 >> -1)
assert raisesException(lambda: a << -1)
assert raisesException(lambda: a >> -1)

print('ok')
