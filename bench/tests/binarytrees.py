# args: 6
# output: stretch tree of depth 7	 check: -1
# output: 128	 trees of depth 4	 check: -128
# output: 32	 trees of depth 6	 check: -32
# output: long lived tree of depth 6	 check: -1
# bench-args: 11

# The Computer Language Benchmarks Game
# http://benchmarksgame.alioth.debian.org/
#
# contributed by Antoine Pitrou
# modified by Dominique Wahli and Daniel Nanz
# modified by Joerg Baumann

# COMMAND LINE:
# /usr/local/src/Python-3.4.3/bin/python3.4 binarytrees.py 20

# PROGRAM OUTPUT:
# stretch tree of depth 21	 check: -1
# 2097152	 trees of depth 4	 check: -2097152
# 524288	 trees of depth 6	 check: -524288
# 131072	 trees of depth 8	 check: -131072
# 32768	 trees of depth 10	 check: -32768
# 8192	 trees of depth 12	 check: -8192
# 2048	 trees of depth 14	 check: -2048
# 512	 trees of depth 16	 check: -512
# 128	 trees of depth 18	 check: -128
# 32	 trees of depth 20	 check: -32
# long lived tree of depth 20	 check: -1

import sys

def make_tree(i, d):

    if d > 0:
        d -= 1
        return (i, make_tree(i, d), make_tree(i + 1, d))
    return (i, None, None)


def check_tree(node):

    (i, l, r) = node
    if l is None:
        return i
    else:
        return i + check_tree(l) - check_tree(r)


def make_check(itde, make=make_tree, check=check_tree):

    i, d = itde
    return check(make(i, d))


def get_argchunks(i, d, chunksize=5000):

    assert chunksize % 2 == 0
    chunk = []
    for k in range(1, i + 1):
        chunk.extend([(k, d), (-k, d)])
        if len(chunk) == chunksize:
            yield chunk
            chunk = []
    if len(chunk) > 0:
        yield chunk


def main(n, min_depth=4):

    max_depth = max(min_depth + 2, n)
    stretch_depth = max_depth + 1
    chunkmap = map

    print('stretch tree of depth ' + str(stretch_depth) +
          '\t check: ' + str(make_check((0, stretch_depth))))

    long_lived_tree = make_tree(0, max_depth)

    mmd = max_depth + min_depth
    for d in range(min_depth, stretch_depth, 2):
        i = 2 ** (mmd - d)
        cs = 0
        for argchunk in get_argchunks(i,d):
            cs += sum(chunkmap(make_check, argchunk))
        print(str(i * 2) + '\t trees of depth ' + str(d) +
              '\t check: ' + str(cs))

    print('long lived tree of depth ' + str(max_depth) + '\t check: ' + str(check_tree(long_lived_tree)))

if __name__ == '__main__':
    main(int(sys.argv[1]))
