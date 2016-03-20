# output: ok

assert __name__ == "__main__"
assert __package__ == ""

assert __builtins__.__name__ == "builtins"
assert __builtins__.__package__ == ""

import sys;
assert isinstance(sys.argv, list)
assert sys.__name__ == "sys"
assert sys.__package__ == ""

import bisect
assert bisect.__name__ == "bisect"

assert bisect.bisect_right([1, 1, 1, 1], 2) == 4

import bisect as alias1, bisect as alias2
assert alias1.bisect_right([1, 1, 1, 1], 0) == 0
assert alias2.bisect_right([1, 1, 1, 1], 4) == 4

from bisect import bisect_right as f1
assert f1([1, 1, 2, 2], 0) == 0

from bisect import bisect_right as f2, bisect_right as f3
assert f2([1, 1, 2, 2], 1) == 2
assert f3([1, 1, 2, 2], 1.5) == 2

print('ok')
