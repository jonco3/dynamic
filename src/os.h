#ifndef __OS_H__
#define __OS_H__

#include <cstdio>

enum class SeekWhence
{
    Set = SEEK_SET,
    Cur = SEEK_CUR,
    End = SEEK_END
};

const int MinSeekWhence = int(SeekWhence::Set);
const int MaxSeekWhence = int(SeekWhence::End);

#endif
