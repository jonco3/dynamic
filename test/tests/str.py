# output: ok

assert str(None) == 'None'
assert str(1) == '1'
assert str(1.2) == '1.2'
assert str('a') == 'a'
assert str(()) == '()'
assert str((1,)) == '(1,)'
assert str(('a',)) == "('a',)"
assert str((1, 2, 3)) == '(1, 2, 3)'
assert str([]) == '[]'
assert str(['a']) == "['a']"
assert str([1, 2, 3]) == '[1, 2, 3]'
assert str({}) == '{}'
assert str({'a': 1}) == "{'a': 1}"
assert "<object object at" in str(object())

assert repr(None) == 'None'
assert repr(1) == '1'
assert repr(1.2) == '1.2'
assert repr('a') == "'a'"
assert repr(()) == '()'
assert repr(('a',)) == "('a',)"
assert repr((1,)) == '(1,)'
assert repr((1, 2, 3)) == '(1, 2, 3)'
assert repr([]) == '[]'
assert repr(['a']) == "['a']"
assert repr([1, 2, 3]) == '[1, 2, 3]'
assert repr({}) == '{}'
assert repr({'a': 1}) == "{'a': 1}"
assert "<object object at" in repr(object())

class A:
    def __repr__(self):
        return 'repr'

class B:
    def __str__(self):
        return 'str'

class C:
    def __repr__(self):
        return 'repr'
    def __str__(self):
        return 'str'

assert repr(A()) == 'repr'
assert str(A()) == 'repr'
assert "object at" in repr(B())
assert str(B()) == 'str'
assert repr(C()) == 'repr'
assert str(C()) == 'str'

print('ok')
