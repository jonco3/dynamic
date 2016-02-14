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

a = "a"
a += "b"
assert a == "ab"

assert(len("abc") == 3)
assert("abc"[1] == "b")
assert([x for x in "abc"] == ["a", "b", "c"])

exc = False
try:
    a = 1 + "foo"
except TypeError:
    exc = True
assert(exc)

exc = False
try:
    a = "foo" + 1
except TypeError:
    exc = True
assert(exc)

assert("foo" + str(1) == "foo1")

# split

assert("foo bar".split(" ") == ["foo", "bar"])
assert("foo bar".split("o") == ['f', '', ' bar'])
assert("fo".split("o") == ['f', ''])
assert("of".split("o") == ['', 'f'])
assert("o".split("o") == ['', ''])
assert("".split("o") == [''])

assert("foo bar".split() == ["foo", "bar"])
assert("foo\n \nbar".split() == ["foo", "bar"])
assert(" foo".split() == ["foo"])
assert("foo ".split() == ["foo"])
assert("    ".split() == [])
assert("".split() == [])

# join

assert("".join(()) == "")
assert("".join(["a", "b", "c"]) == "abc")
assert("_".join(["a", "b", "c"]) == "a_b_c")
assert("_".join(["", "b", ""]) == "_b_")

# multiline strings

assert """foo
bar""" == "foo\nbar"

print('ok')
