
struct Object
{
};

struct Block
{
    std::vector<Instr *> instrs;
};

struct Function
{
    std::auto_ptr<Block> block;
};

struct Class
{
};
