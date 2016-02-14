# output: ok

import sys;
assert isinstance(sys.argv, list)

import bisect
assert bisect.bisect_right([1, 1, 1, 1], 2) == 4

print('ok')
