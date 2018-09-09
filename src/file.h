#ifndef __FILE_H__
#define __FILE_H__

#include "object.h"
#include "os.h"

#include "stdio.h"

struct Dict;

struct File : public InlineObject<4>
{
    static GlobalRoot<Class*> ObjectClass;

    static void Init();

    static bool Open(NativeArgs args, MutableTraced<Value> resultOut);

    File(FILE* file, Traced<String*> name, Traced<String*> mode);

    bool closed() const;
    int fileno() const;
    bool isatty() const;
    int64_t tell() const;
    // String* encoding();
    // Object* errors();
    // Object* newlines() const;

    void close();
    void flush();
    // Object* next();
    bool read(size_t size, MutableTraced<Value> resultOut);
    // Object* readline(int64_t size = -1);
    // Object* readlines(int64_t sizeHint = -1);
    // Object* xreadlines();
    bool seek(int64_t offset, SeekWhence whence,
              MutableTraced<Value> resultOut);
    bool truncate(int64_t size, MutableTraced<Value> resultOut);
    bool write(Traced<String*> str, MutableTraced<Value> resultOut);
    // void writelines(Traced<Object*> seq);

  private:
    enum {
        NameSlot = 0,
        ModeSlot,
        ClosedSlot,
        SoftSpaceSlot
    };

    friend void initFiles();
    static bool New(NativeArgs args, MutableTraced<Value> resultOut);

    bool checkError(string what, MutableTraced<Value> resultOut);
    bool createError(string what, MutableTraced<Value> resultOut);

    static GlobalRoot<Layout*> InitialLayout;

    FILE* file_;
};

#endif
