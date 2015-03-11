#ifndef __UTILS_H__
#define __UTILS_H__

struct AutoSetAndRestore
{
    AutoSetAndRestore(bool& var, bool newValue)
      : var_(var), prevValue_(var)
    {
        var_ = newValue;
    }

    ~AutoSetAndRestore() {
        var_ = prevValue_;
    }

  private:
    bool& var_;
    bool prevValue_;
};

#endif
