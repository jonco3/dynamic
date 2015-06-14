#ifndef __NAME_H__
#define __NAME_H__

/*
 * Currently just use std::sring for names, but this could be changed to
 * something more lightweight.
 */

#include <string>

using namespace std;

struct Name
{
    Name(const string& str)
      : string_(intern(str)) {}

    Name(const char* str)
      : string_(intern(str)) {}

    bool operator==(const Name& other) const {
        return string_ == other.string_;
    }
    bool operator!=(const Name& other) const {
        return string_ != other.string_;
    }

    bool operator==(const string& other) const {
        return get() == other;
    }
    bool operator!=(const string& other) const {
        return get() != other;
    }

    bool operator==(const char* other) const {
        return get() == other;
    }
    bool operator!=(const char* other) const {
        return get() != other;
    }

    const string& get() const { return *string_; }
    operator const string&() const { return get(); }

    size_t asBits() { return size_t(string_); }

  private:
    const string* string_;

    static string* intern(const string& s);
};

inline ostream& operator<<(ostream& s, Name name)
{
    s << name.get();
    return s;
}

inline string operator+(const string& a, Name b) {
    return a + b.get();
}

template<>
struct hash<Name>
{
    typedef Name argument_type;
    typedef std::size_t result_type;

    size_t operator()(Name n) const
    {
        return n.asBits();
    }
};

extern void initNames();
extern void shutdownNames();

#endif
