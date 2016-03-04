#ifndef __ASSERT_H__
#define __ASSERT_H__

#ifdef NDEBUG

#define assert(ignore)((void) 0)

#else

#define breakpoint()                                                          \
    for(size_t i = 0; i < 3; i++)                                             \
        __asm__("int3")

#define assert(cond)                                                          \
    do {                                                                      \
        if (!(cond)) {                                                        \
            printAssertMessage(#cond, __FILE__, __LINE__);                    \
            breakpoint();                                                     \
            crash();                                                          \
        }                                                                     \
    } while(false)

extern void printAssertMessage(const char* cond, const char* file,
                               unsigned line);

#endif // NDEBUG

extern void crash(const char* message = nullptr);

#endif
