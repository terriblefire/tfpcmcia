#include "debug.h"

#if !PROD

#include <exec/types.h>
#include <proto/exec.h>
#include <stdarg.h>

static void rawPutChar(register UBYTE c __asm("d0"))
{
    register struct ExecBase* const a6 __asm("a6") = *(struct ExecBase**)4;
    register ULONG d0 __asm("d0") = c;
    __asm volatile ("jsr a6@(-516:W)"
        :
        : "r"(a6), "r"(d0)
        : "d0", "d1", "a0", "a1", "cc", "memory");
}

void kprintf(const char* format, ...)
{
    if (!format)
        return;

    va_list arg;
    va_start(arg, format);
    struct ExecBase* SysBase = *(struct ExecBase**)4;
    RawDoFmt((STRPTR)format, arg, (void(*)(void))rawPutChar, 0);
    va_end(arg);
}

#endif
