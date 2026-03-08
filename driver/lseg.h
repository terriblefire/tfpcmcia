#pragma once

#include <exec/types.h>
#include <dos/dos.h>

typedef ULONG (*ReadFunc)(void* readHandle, void* buffer, ULONG length);
typedef void* (*AllocVecFunc)(ULONG size, ULONG flags);
typedef void (*FreeVecFunc)(void* memory);

struct LoadSegFuncs
{
    ReadFunc     read;
    AllocVecFunc alloc;
    FreeVecFunc  free;
};

BPTR CustomLoadSeg(void* fh, BPTR table, struct LoadSegFuncs* funcs);
