# output: ok

# todo: set
# todo: user defined with __contains__, with __iter__, __getitem__

assert 2 in [1, 2, 3]
assert 0 not in [1, 2, 3]
assert 0 not in []

assert "a" in ["a"]
assert "a" + "b" in ["ab"]

assert 2 in (1, 2, 3)
assert 0 not in (1, 2, 3)
assert 0 not in ()

assert "f" + "oo" in {'foo': 0, 'bar': 1}
assert "baz" not in {'foo': 0, 'bar': 1}
assert "baz" not in {}

assert "" in "any"
assert "n" in "any"
assert "t" not in "any"

print("ok")
