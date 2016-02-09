# output: ok

# Test closed attribute
f = open("test/data/helloShort.txt")
assert not f.closed
f.close()
assert f.closed
f.close()
assert f.closed

# Test isatty()
f = open("test/data/helloShort.txt")
assert not f.isatty()
f.close()

# Test fileno()
f = open("test/data/helloShort.txt")
g = open("test/data/helloShort.txt")
x, y = f.fileno(), g.fileno()
assert isinstance(x, int)
assert x > 2
assert x != y
f.close()
g.close()

# Read whole file
f = open("test/data/helloShort.txt")
assert f.read() == "hello world\n"
f.close()

f = open("test/data/helloLong.txt")
assert len(f.read()) == 4914
f.close()

# Read partial file
f = open("test/data/helloLong.txt")
count = 0
while True:
    s = f.read(13)
    if s == "":
        break
    assert s == "hello world!\n"
    count += 1
assert count == 378

# Test seek() and tell()
f = open("test/data/helloShort.txt")
assert f.tell() == 0
f.seek(6)
assert f.tell() == 6
assert f.read() == "world\n"
assert f.tell() == 12
f.seek(1)
assert f.read(4) == "ello"
assert f.tell() == 5
# todo: test whence argument
f.close()

# todo: truncate
# todo: write

# Test mode and name attributes
f = open("test/data/helloShort.txt")
assert f.mode == "r"
assert f.name == "test/data/helloShort.txt"
f.close()

print('ok')

