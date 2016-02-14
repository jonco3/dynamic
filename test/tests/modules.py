# output: ok

import sys;
assert isinstance(sys.argv, list)

import bisect
assert bisect.bisect_right([1, 1, 1, 1], 2) == 4

import bisect as alias1, bisect as alias2
assert alias1.bisect_right([1, 1, 1, 1], 0) == 0
assert alias2.bisect_right([1, 1, 1, 1], 4) == 4

print('ok')
