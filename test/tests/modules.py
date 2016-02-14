# output: ok

import sys;
assert isinstance(sys.argv, list)

import bisect
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
