#include "file.h"

#include "exception.h"
#include "os.h"

#include "value-inl.h"

GlobalRoot<Class*> File::ObjectClass;
GlobalRoot<Layout*> File::InitialLayout;

static bool FileFileNo(NativeArgs args, MutableTraced<Value> resultOut)
{
    if (!checkInstanceOf(args[0], File::ObjectClass, resultOut))
        return false;

    resultOut = Integer::get(args[0].as<File>()->fileno());
    return true;
}

static bool FileIsATTY(NativeArgs args, MutableTraced<Value> resultOut)
{
    if (!checkInstanceOf(args[0], File::ObjectClass, resultOut))
        return false;

    resultOut = Boolean::get(args[0].as<File>()->isatty());
    return true;
}

static bool FileTell(NativeArgs args, MutableTraced<Value> resultOut)
{
    if (!checkInstanceOf(args[0], File::ObjectClass, resultOut))
        return false;

    resultOut = Integer::get(args[0].as<File>()->tell());
    return true;
}

static bool FileClose(NativeArgs args, MutableTraced<Value> resultOut)
{
    if (!checkInstanceOf(args[0], File::ObjectClass, resultOut))
        return false;

    args[0].as<File>()->close();
    resultOut = None;
    return true;
}

static bool FileFlush(NativeArgs args, MutableTraced<Value> resultOut)
{
    if (!checkInstanceOf(args[0], File::ObjectClass, resultOut))
        return false;

    args[0].as<File>()->flush();
    resultOut = None;
    return true;
}

static bool ConvertSize(Traced<Value> arg, size_t& sizeOut,
                        MutableTraced<Value> resultOut)
{
    if (!checkInstanceOf(arg, Integer::ObjectClass, resultOut))
        return false;

    if (!arg.toSize(sizeOut)) {
        resultOut = gc.create<ValueError>("Size out of range");
        return false;
    }

    return true;
}

static bool FileRead(NativeArgs args, MutableTraced<Value> resultOut)
{
    if (!checkInstanceOf(args[0], File::ObjectClass, resultOut))
        return false;

    size_t size = SIZE_MAX;
    if (args.size() == 2) {
        if (!ConvertSize(args[1], size, resultOut))
            return false;
    }

    return args[0].as<File>()->read(size, resultOut);
}

static bool FileSeek(NativeArgs args, MutableTraced<Value> resultOut)
{
    if (!checkInstanceOf(args[0], File::ObjectClass, resultOut))
        return false;

    size_t offset;
    if (!ConvertSize(args[1], offset, resultOut))
        return false;

    SeekWhence whence = SeekWhence::Set;
    if (args.size() == 3) {
        if (!checkInstanceOf(args[2], Integer::ObjectClass, resultOut))
            return false;

        int i;
        if (!args[2].toInt32(i) || (i < MinSeekWhence || i > MaxSeekWhence)) {
            resultOut = gc.create<ValueError>("whence argumnt out of range");
            return false;
        }

        whence = SeekWhence(i);
    }

    return args[0].as<File>()->seek(offset, whence, resultOut);
}

static bool FileTruncate(NativeArgs args,
                         MutableTraced<Value> resultOut)
{
    if (!checkInstanceOf(args[0], File::ObjectClass, resultOut))
        return false;

    Stack<File*> file(args[0].as<File>());

    size_t size;
    if (args.size() == 2) {
        if (!ConvertSize(args[1], size, resultOut))
            return false;
    } else {
        size = file->tell();
    }

    return file->truncate(size, resultOut);
}

static bool FileWrite(NativeArgs args, MutableTraced<Value> resultOut)
{
    if (!checkInstanceOf(args[0], File::ObjectClass, resultOut) ||
        !checkInstanceOf(args[1], String::ObjectClass, resultOut))
    {
        return false;
    }

    Stack<String*> str(args[1].as<String>());
    return args[0].as<File>()->write(str, resultOut);
}

void File::Init()
{
    ObjectClass.init(Class::createNative("file", New, 1));
    InitialLayout.init(Object::InitialLayout
                       ->addName(Names::name, NameSlot)
                       ->addName(Names::mode, ModeSlot)
                       ->addName(Names::closed, ClosedSlot)
                       ->addName(Names::softSpace, SoftSpaceSlot));
    // todo: name, mode and closed should be readonly attributes

    initNativeMethod(ObjectClass, "fileno", FileFileNo, 1);
    initNativeMethod(ObjectClass, "isatty", FileIsATTY, 1);
    initNativeMethod(ObjectClass, "tell", FileTell, 1);
    initNativeMethod(ObjectClass, "close", FileClose, 1);
    initNativeMethod(ObjectClass, "flush", FileFlush, 1);
    initNativeMethod(ObjectClass, "read", FileRead, 1, 2);
    initNativeMethod(ObjectClass, "seek", FileSeek, 2, 3);
    initNativeMethod(ObjectClass, "truncate", FileTruncate, 2);
    initNativeMethod(ObjectClass, "write", FileWrite, 2);
};

/* static */ bool File::New(NativeArgs args,
                            MutableTraced<Value> resultOut)
{
    resultOut = gc.create<NotImplementedError>("Can't create file");
    return false;
}

/* static */ bool File::Open(NativeArgs args,
                             MutableTraced<Value> resultOut)
{
    assert(args.size() == 1 || args.size() == 2);
    if (!args[0].is<String>() ||
        (args.size() == 2 && !args[0].is<String>()))
    {
        resultOut = gc.create<TypeError>("Expecting string arguments");
        return false;
    }

    Stack<String*> filenameStr(args[0].as<String>());

    Stack<String*> modeStr;
    if (args.size() == 2)
        modeStr = args[1].as<String>();
    else
        modeStr = String::get("r");

    string mode = modeStr->value();
    if (mode != "r" && mode != "w" && mode != "a") {
        // todo: implement other open modes
        resultOut = gc.create<TypeError>("Bad file mode '" + mode + "'");
        return false;
    }

    string filename = filenameStr->value();
    FILE* f = fopen(filename.c_str(), mode.c_str());
    if (!f) {
        resultOut = gc.create<OSError>(
            "Error opening file '" + filename + "': " + strerror(errno));
        return false;
    }

    resultOut = gc.create<File>(f, filenameStr, modeStr);
    return true;
}

File::File(FILE* file, Traced<String*> name, Traced<String*> mode)
  : Object(ObjectClass, InitialLayout),
    file_(file)
{
    assert(file_);
    setSlot(NameSlot, Value(name));
    setSlot(ModeSlot, Value(mode));
    setSlot(ClosedSlot, Value(Boolean::False));
    setSlot(SoftSpaceSlot, Value(None));
}

bool File::closed() const
{
    Stack<Value> value(getSlot(ClosedSlot));
    return Value::IsTrue(value);
}

int File::fileno() const
{
    return ::fileno(file_);
}

bool File::isatty() const
{
    return ::isatty(fileno());
}

int64_t File::tell() const
{
    return ftell(file_);
}

void File::close()
{
    if (!closed()) {
        fclose(file_);
        setSlot(ClosedSlot, Value(Boolean::True));
    }
}

void File::flush()
{
    fflush(file_);
}

bool File::checkError(string what, MutableTraced<Value> resultOut)
{
    if (!ferror(file_))
        return true;

    return createError(what, resultOut);
}

bool File::createError(string what, MutableTraced<Value> resultOut)
{
    resultOut = gc.create<OSError>(what + ": " + strerror(errno));
    return false;
}

bool File::read(size_t size, MutableTraced<Value> resultOut)
{
    const size_t BufferSize = 1024;

    char buffer[BufferSize];
    string result;

    while (result.size() < size && !ferror(file_) && !feof(file_)) {
        size_t toRead = min(size - result.size(), BufferSize);
        size_t r = fread(buffer, 1, toRead, file_);
        if (r)
            result.append(buffer, r);
    }

    if (!checkError("Read error", resultOut))
        return false;

    resultOut = gc.create<String>(result);
    return true;
}

bool File::seek(int64_t offset, SeekWhence whence,
                MutableTraced<Value> resultOut)
{
    if (fseek(file_, offset, int(whence)))
        return createError("Seek error", resultOut);

    resultOut = None;
    return true;
}

bool File::truncate(int64_t size, MutableTraced<Value> resultOut)
{
    if (ftruncate(fileno(), size))
        return createError("Truncate error", resultOut);

    resultOut = None;
    return true;
}

bool File::write(Traced<String*> str, MutableTraced<Value> resultOut)
{
    const string& s = str->value();
    size_t r = fwrite(s.c_str(), 1, s.size(), file_);
    if (r != s.size())
        return createError("Write error", resultOut);

    resultOut = None;
    return true;
}

