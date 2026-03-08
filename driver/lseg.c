#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/doshunks.h>

#include "lseg.h"
#include "debug.h"

static ULONG ReadLong(ReadFunc read, void* fh)
{
    ULONG v;
    if (read(fh, &v, sizeof(v)) != sizeof(v))
        return 0;
    return v;
}

static ULONG ReadWord(ReadFunc read, void* fh)
{
    UWORD v;
    if (read(fh, &v, sizeof(v)) != sizeof(v))
        return 0;
    return v;
}

static ULONG ReadSize(ReadFunc read, void* fh, ULONG* memFlags)
{
    ULONG size = ReadLong(read, fh);

    ULONG flags = size >> 29;
    if ((flags & 0x3) == 0x3)
    {
        flags = ReadLong(read, fh);
    }
    *memFlags = flags;

    size <<= 2; // mask to 30 bits, multiply by sizeof(ULONG)
    return size;
}

BPTR CustomLoadSeg(void* fh, BPTR table, struct LoadSegFuncs* funcs)
{
    BPTR* segTable = (BPTR*)BADDR(table);
    BPTR segList = 0;

    // Check for HUNK_HEADER
    if (ReadLong(funcs->read, fh) != HUNK_HEADER)
    {
        kprintf("LSEG: not hunk format\n");
        return 0;
    }

    kprintf("LSEG: HUNK_HEADER\n");

    // Skip resident library names (must be 0)
    if (ReadLong(funcs->read, fh))
    {
        kprintf("LSEG: resident lib not supported\n");
        return 0;
    }

    ULONG memFlags = 0;
    ULONG tableSize = ReadSize(funcs->read, fh, &memFlags);
    ULONG firstHunk = ReadLong(funcs->read, fh);
    ULONG lastHunk = ReadLong(funcs->read, fh);

    kprintf("LSEG: %ld hunks (%ld to %ld)\n", lastHunk - firstHunk + 1, firstHunk, lastHunk);

    if (!segTable)
    {
        memFlags |= MEMF_CLEAR;
        segTable = (BPTR*)funcs->alloc(tableSize, memFlags);
        if (!segTable)
        {
            kprintf("LSEG: alloc segTable failed\n");
            return 0;
        }
    }

    // Allocate hunks and build segment list
    {
        BPTR* currentHunk = &segTable[firstHunk];
        BPTR* nextSeg = &segList;
        for (ULONG i = firstHunk; i <= lastHunk; i++)
        {
            ULONG hunkSize = ReadSize(funcs->read, fh, &memFlags);
            kprintf("LSEG: hunk %ld: %ld bytes\n", i, hunkSize);

            void* hunkPtr = funcs->alloc(hunkSize + sizeof(BPTR), memFlags);
            if (!hunkPtr)
            {
                kprintf("LSEG: alloc hunk %ld failed\n", i);
                return 0;
            }
            BPTR bptr = MKBADDR(hunkPtr);
            *currentHunk++ = bptr;
            *nextSeg = bptr;
            nextSeg = (BPTR*)hunkPtr;
        }
        *nextSeg = 0;
    }

    ULONG hunkIndex = firstHunk;

    while (1)
    {
        ULONG hunkType = ReadLong(funcs->read, fh);
        if (!hunkType)
            break;

        hunkType &= 0x3FFFFFFF;

        switch (hunkType)
        {
            case HUNK_CODE:
            case HUNK_DATA:
            {
                ULONG size = ReadSize(funcs->read, fh, &memFlags);
                ULONG* dest = (ULONG*)BADDR(segTable[hunkIndex] + 1);
                funcs->read(fh, dest, size);
                break;
            }

            case HUNK_BSS:
            {
                ULONG size = ReadSize(funcs->read, fh, &memFlags);
                // Memory was allocated with MEMF_CLEAR, nothing to do
                (void)size;
                break;
            }

            case HUNK_RELOC32:
            {
                UBYTE* currentHunk = (UBYTE*)BADDR(segTable[hunkIndex] + 1);

                while (1)
                {
                    ULONG count = ReadSize(funcs->read, fh, &memFlags);
                    if (!count)
                        break;
                    count >>= 2;
                    ULONG hunkNr = ReadLong(funcs->read, fh);
                    ULONG baseAddr = (ULONG)BADDR(segTable[hunkNr] + 1);

                    while (count--)
                    {
                        ULONG offset = ReadLong(funcs->read, fh);
                        ULONG* reloc = (ULONG*)(&currentHunk[offset]);
                        *reloc += baseAddr;
                    }
                }
                break;
            }

            // V37 LoadSeg uses HUNK_DREL32 (1015) in place of
            // HUNK_RELOC32SHORT (1020) by mistake.  Both use 16-bit
            // count/hunk/offset fields with long-word padding.
            case HUNK_DREL32:
            case HUNK_RELOC32SHORT:
            {
                UBYTE* currentHunk = (UBYTE*)BADDR(segTable[hunkIndex] + 1);

                ULONG wordsRead = 0;
                while (1)
                {
                    ULONG count = ReadWord(funcs->read, fh); ++wordsRead;
                    if (!count)
                        break;
                    ULONG hunkNr = ReadWord(funcs->read, fh); ++wordsRead;
                    ULONG baseAddr = (ULONG)BADDR(segTable[hunkNr] + 1);
                    while (count--)
                    {
                        ULONG offset = ReadWord(funcs->read, fh); ++wordsRead;
                        ULONG* reloc = (ULONG*)(&currentHunk[offset]);
                        *reloc += baseAddr;
                    }
                }
                // pad to longword if odd number of words read
                if (wordsRead & 1)
                    ReadWord(funcs->read, fh);
                break;
            }

            case HUNK_RELRELOC32:
            {
                UBYTE* currentHunk = (UBYTE*)BADDR(segTable[hunkIndex] + 1);

                ULONG wordsRead = 0;
                while (1)
                {
                    ULONG count = ReadWord(funcs->read, fh); ++wordsRead;
                    if (!count)
                        break;
                    ULONG hunkNr = ReadWord(funcs->read, fh); ++wordsRead;
                    ULONG baseAddr = (ULONG)BADDR(segTable[hunkNr] + 1);
                    while (count--)
                    {
                        ULONG offset = ReadWord(funcs->read, fh); ++wordsRead;
                        ULONG* reloc = (ULONG*)(&currentHunk[offset]);
                        *reloc -= (ULONG)reloc;
                        *reloc += baseAddr;
                    }
                }
                // pad to longword if odd number of words read
                if (wordsRead & 1)
                    ReadWord(funcs->read, fh);
                break;
            }

            case HUNK_SYMBOL:
            {
                // Skip symbol data
                while (1)
                {
                    ULONG nameLen = ReadLong(funcs->read, fh);
                    if (!nameLen)
                        break;
                    // Skip name longs + value long
                    for (ULONG i = 0; i < nameLen; i++)
                        ReadLong(funcs->read, fh);
                    ReadLong(funcs->read, fh); // value
                }
                break;
            }

            case HUNK_DEBUG:
            {
                ULONG size = ReadSize(funcs->read, fh, &memFlags);
                size >>= 2;
                while (size--)
                    ReadLong(funcs->read, fh);
                break;
            }

            case HUNK_END:
                hunkIndex++;
                break;

            default:
                kprintf("LSEG: unknown hunk $%lx\n", hunkType);
                return 0;
        }
    }

    funcs->free(segTable);

    kprintf("LSEG: segList = %08lx\n", segList);
    return segList;
}
