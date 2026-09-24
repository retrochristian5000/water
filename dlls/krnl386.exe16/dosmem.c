/*
 * DOS memory emulation
 *
 * Copyright 1995 Alexandre Julliard
 * Copyright 1996 Marcus Meissner
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "windef.h"
#include "winbase.h"
#include "excpt.h"
#include "winternl.h"
#include "wine/winbase16.h"

#include "kernel16_private.h"
#include "dosexe.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(dosmem);
WINE_DECLARE_DEBUG_CHANNEL(selector);

WORD DOSMEM_0000H;        /* segment at 0:0 */
WORD DOSMEM_BiosDataSeg;  /* BIOS data segment at 0x40:0 */
WORD DOSMEM_BiosSysSeg;   /* BIOS ROM segment at 0xf000:0 */

WORD DOSVM_psp = 0;
WORD int16_sel = 0;

/* DOS memory highest address (including HMA) */
#define DOSMEM_SIZE             0x110000
#define DOSMEM_64KB             0x10000
#define DOSMEM_UMB_BOTTOM       0x0d0000
#define DOSMEM_UMB_TOP          0x0effff

/*
 * Memory Control Block (MCB) definition
 */

#define MCB_DUMP(mc) \
    TRACE ("MCB_DUMP base=%p type=%02xh psp=%04xh size=%04xh\n", mc, mc->type, mc->psp , mc->size )

#define MCB_NEXT(mc) \
    (MCB*) ((mc->type==MCB_TYPE_LAST) ? NULL : (char*)(mc) + ((mc->size + 1) << 4) )

/* FIXME: should we check more? */
#define MCB_VALID(mc) \
    ((mc->type==MCB_TYPE_NORMAL) || (mc->type==MCB_TYPE_LAST))


#define MCB_TYPE_NORMAL    0x4d
#define MCB_TYPE_LAST      0x5a

#define MCB_PSP_DOS        0x0060
#define MCB_PSP_FREE       0

#pragma pack(push,1)
typedef struct {
    BYTE type;
    WORD psp;     /* segment of owner psp */
    WORD size;    /* in paragraphs */
    BYTE pad[3];
    BYTE name[8];
} MCB;
#pragma pack(pop)

/*
#define __DOSMEM_DEBUG__
 */

#define VM_STUB(x) (0x90CF00CD|(x<<8)) /* INT x; IRET; NOP */
#define VM_STUB_SEGMENT 0xf000         /* BIOS segment */

/* FIXME: the conventional-memory root should be moved to the LOL. */
static MCB *DOSMEM_root_block;

#define DOSMEM_UMB_MAX_BLOCKS 64
#define DOSMEM_UMB_RESERVED   0xffff

typedef struct
{
    WORD segment;
    WORD size;     /* paragraphs */
    WORD psp;      /* 0 = free, 0xffff = reserved */
} UMB_BLOCK;

static UMB_BLOCK DOSMEM_umb_blocks[DOSMEM_UMB_MAX_BLOCKS];
static unsigned int DOSMEM_umb_count;
static BOOL DOSMEM_umb_initialized;

/* when looking at DOS and real mode memory, we activate in three different
 * modes, depending the situation.
 * 1/ By default (protected mode), the first MB of memory (actually 0x110000,
 *    when you also look at the HMA part) is always reserved, whatever you do.
 *    We allocated some PM selectors to this memory, even if this area is not
 *    committed at startup
 * 2/ if a program tries to use the memory through the selectors, we actually
 *    commit this memory, made of: BIOS segment, but also some system 
 *    information, usually low in memory that we map for the circumstance also
 *    in the BIOS segment, so that we keep the low memory protected (for NULL
 *    pointer deref catching for example). In this case, we're still in PM
 *    mode, accessing part of the "physical" real mode memory. In fact, we don't
 *    map all the first meg, we keep 64k uncommitted to still catch NULL 
 *    pointers dereference
 * 3/ if the process enters the real mode, then we (also) commit the full first
 *    MB of memory (and also initialize the DOS structures in it).
 */

/* DOS memory base (linear in process address space) */
static char *DOSMEM_dosmem;
static char *DOSMEM_sysmem;
/* number of bytes protected from _dosmem. 0 when DOS memory is initialized, 
 * 64k otherwise to trap NULL pointers deref */
static DWORD DOSMEM_protect;

static LONG WINAPI dosmem_handler(EXCEPTION_POINTERS* except);
static void *vectored_handler;

/***********************************************************************
 *           DOSMEM_FillIsrTable
 *
 * Fill the interrupt table with fake BIOS calls to BIOSSEG (0xf000).
 *
 * NOTES:
 * Linux normally only traps INTs performed from or destined to BIOSSEG
 * for us to handle, if the int_revectored table is empty. Filling the
 * interrupt table with calls to INT stubs in BIOSSEG allows DOS programs
 * to hook interrupts, as well as use their familiar retf tricks to call
 * them, AND let Wine handle any unhooked interrupts transparently.
 */
static void DOSMEM_FillIsrTable(void)
{
    SEGPTR *isr = (SEGPTR*)DOSMEM_sysmem;
    int x;

    for (x=0; x<256; x++) isr[x]=MAKESEGPTR(VM_STUB_SEGMENT,x*4);
}

static void DOSMEM_MakeIsrStubs(void)
{
    DWORD *stub = (DWORD*)(DOSMEM_dosmem + (VM_STUB_SEGMENT << 4));
    int x;

    for (x=0; x<256; x++) stub[x]=VM_STUB(x);
}

BIOSDATA* DOSVM_BiosData(void)
{
    return (BIOSDATA *)(DOSMEM_sysmem + 0x400);
}

/**********************************************************************
 *          DOSMEM_GetTicksSinceMidnight
 *
 * Return number of clock ticks since midnight.
 */
static DWORD DOSMEM_GetTicksSinceMidnight(void)
{
    SYSTEMTIME time;

    /* This should give us the (approximately) correct
     * 18.206 clock ticks per second since midnight.
     */

    GetLocalTime( &time );

    return (((time.wHour * 3600 + time.wMinute * 60 +
              time.wSecond) * 18206) / 1000) +
             (time.wMilliseconds * 1000 / 54927);
}

/***********************************************************************
 *           DOSMEM_FillBiosSegments
 *
 * Fill the BIOS data segment with dummy values.
 */
static void DOSMEM_FillBiosSegments(void)
{
    BYTE *pBiosSys = (BYTE*)DOSMEM_dosmem + 0xf0000;
    BYTE *pBiosROMTable = pBiosSys+0xe6f5;
    BIOSDATA *pBiosData = DOSVM_BiosData();
    static const char bios_date[] = "13/01/99";

      /* Clear all unused values */
    memset( pBiosData, 0, sizeof(*pBiosData) );

    /* FIXME: should check the number of configured drives and ports */
    pBiosData->Com1Addr             = 0x3f8;
    pBiosData->Com2Addr             = 0x2f8;
    pBiosData->Lpt1Addr             = 0x378;
    pBiosData->Lpt2Addr             = 0x278;
    pBiosData->InstalledHardware    = 0x5463;
    pBiosData->MemSize              = 640;
    pBiosData->NextKbdCharPtr       = 0x1e;
    pBiosData->FirstKbdCharPtr      = 0x1e;
    pBiosData->VideoMode            = 3;
    pBiosData->VideoColumns         = 80;
    pBiosData->VideoPageSize        = 80 * 25 * 2;
    pBiosData->VideoPageStartAddr   = 0xb800;
    pBiosData->VideoCtrlAddr        = 0x3d4;
    pBiosData->Ticks                = DOSMEM_GetTicksSinceMidnight();
    pBiosData->NbHardDisks          = 2;
    pBiosData->KbdBufferStart       = 0x1e;
    pBiosData->KbdBufferEnd         = 0x3e;
    pBiosData->RowsOnScreenMinus1   = 24;
    pBiosData->BytesPerChar         = 0x10;
    pBiosData->ModeOptions          = 0x64;
    pBiosData->FeatureBitsSwitches  = 0xf9;
    pBiosData->VGASettings          = 0x51;
    pBiosData->DisplayCombination   = 0x08;
    pBiosData->DiskDataRate         = 0;

    /* fill ROM configuration table (values from Award) */
    *(pBiosROMTable+0x0)	= 0x08; /* number of bytes following LO */
    *(pBiosROMTable+0x1)	= 0x00; /* number of bytes following HI */
    *(pBiosROMTable+0x2)	= 0xfc; /* model */
    *(pBiosROMTable+0x3)	= 0x01; /* submodel */
    *(pBiosROMTable+0x4)	= 0x00; /* BIOS revision */
    *(pBiosROMTable+0x5)	= 0x74; /* feature byte 1 */
    *(pBiosROMTable+0x6)	= 0x00; /* feature byte 2 */
    *(pBiosROMTable+0x7)	= 0x00; /* feature byte 3 */
    *(pBiosROMTable+0x8)	= 0x00; /* feature byte 4 */
    *(pBiosROMTable+0x9)	= 0x00; /* feature byte 5 */

    /* BIOS date string */
    memcpy(pBiosSys+0xfff5, bios_date, sizeof bios_date);

    /* BIOS ID */
    *(pBiosSys+0xfffe) = 0xfc;

    /* Reboot vector (f000:fff0 or ffff:0000) */
    *(DWORD*)(pBiosSys + 0xfff0) = VM_STUB(0x19);
}

/***********************************************************************
 *           BiosTick
 *
 * Increment the BIOS tick counter. Called by timer signal handler.
 */
static void CALLBACK BiosTick( LPVOID arg, DWORD low, DWORD high )
{
    BIOSDATA *pBiosData = arg;
    pBiosData->Ticks++;
}

/***********************************************************************
 *           timer_thread
 */
static DWORD CALLBACK timer_thread( void *arg )
{
    LARGE_INTEGER when;
    HANDLE timer;

    if (!(timer = CreateWaitableTimerA( NULL, FALSE, NULL ))) return 0;

    when.u.LowPart = when.u.HighPart = 0;
    SetWaitableTimer( timer, &when, 55 /* actually 54.925 */, BiosTick, arg, FALSE );
    for (;;) SleepEx( INFINITE, TRUE );
}

/***********************************************************************
 *           DOSVM_start_bios_timer
 *
 * Start the BIOS ticks timer when the app accesses selector 0x40.
 */
void DOSVM_start_bios_timer(void)
{
    static LONG running;

    if (!InterlockedExchange( &running, 1 ))
        CloseHandle( CreateThread( NULL, 0, timer_thread, DOSVM_BiosData(), 0, NULL ));
}

/***********************************************************************
 *           DOSMEM_Collapse
 *
 * Helper function for internal use only.
 * Attach all following free blocks to this one, even if this one is not free.
 */
static void DOSMEM_Collapse( MCB* mcb )
{
    MCB* next = MCB_NEXT( mcb );

    while (next && next->psp == MCB_PSP_FREE)
    {
        mcb->size = mcb->size + next->size + 1;
        mcb->type = next->type;    /* make sure keeping MCB_TYPE_LAST */
        next = MCB_NEXT( next );
    }
}


/***********************************************************************
 *           DOSMEM_InitSegments
 */
static void DOSMEM_InitSegments(void)
{
    LPSTR ptr;
    int   i;

    /*
     * PM / offset N*5: Interrupt N in 16-bit protected mode.
     */
    int16_sel = GLOBAL_Alloc( GMEM_FIXED, 5 * 256, 0, code16_segment );
    ptr = GlobalLock16( int16_sel );
    for(i=0; i<256; i++) {
        /*
         * Each 16-bit interrupt handler is 5 bytes:
         * 0xCD,<i>       = int <i> (interrupt)
         * 0xCA,0x02,0x00 = ret 2   (16-bit far return and pop 2 bytes / eflags)
         */
        ptr[i * 5 + 0] = 0xCD;
        ptr[i * 5 + 1] = i;
        ptr[i * 5 + 2] = 0xCA;
        ptr[i * 5 + 3] = 0x02;
        ptr[i * 5 + 4] = 0x00;
    }
    GlobalUnlock16( int16_sel );
}

/******************************************************************
 *		DOSMEM_InitDosMemory
 */
BOOL DOSMEM_InitDosMemory(void)
{
    static BOOL done;
    static HANDLE hRunOnce;
    DWORD old_prot;

    if (done) return TRUE;

    /* FIXME: this isn't 100% thread safe, as we won't catch accesses while initializing */
    if (hRunOnce == 0)
    {
	HANDLE hEvent = CreateEventW( NULL, TRUE, FALSE, NULL );
        if (InterlockedCompareExchangePointer( &hRunOnce, hEvent, 0 ) == 0)
	{
            BOOL ret;
            DWORD reserve;

	    /* ok, we're the winning thread */
            if (!(ret = VirtualProtect( DOSMEM_dosmem + DOSMEM_protect,
                                        DOSMEM_SIZE - DOSMEM_protect,
                                        PAGE_READWRITE, &old_prot )))
                ERR("Cannot load access low 1Mb, DOS subsystem unavailable\n");
            RemoveVectoredExceptionHandler( vectored_handler );

            /*
             * Reserve either:
             * - lowest 64k for NULL pointer catching (Win16)
             * - lowest 1k for interrupt handlers and
             *   another 0.5k for BIOS, DOS and intra-application
             *   areas (DOS)
             */
            if (DOSMEM_dosmem != DOSMEM_sysmem)
                reserve = 0x10000; /* 64k */
            else
                reserve = 0x600; /* 1.5k */

            /*
             * Set DOS memory base and initialize conventional memory.
             */
            DOSMEM_FillBiosSegments();
            DOSMEM_FillIsrTable();

            /* align root block to paragraph */
            DOSMEM_root_block = (MCB*)(DOSMEM_dosmem + reserve);
            DOSMEM_root_block->type = MCB_TYPE_LAST;
            DOSMEM_root_block->psp = MCB_PSP_FREE;
            DOSMEM_root_block->size = (DOSMEM_dosmem + 0x9fffc  - ((char*)DOSMEM_root_block)) >> 4;

            DOSMEM_umb_blocks[0].segment = DOSMEM_UMB_BOTTOM >> 4;
            DOSMEM_umb_blocks[0].size = (DOSMEM_UMB_TOP + 1 - DOSMEM_UMB_BOTTOM) >> 4;
            DOSMEM_umb_blocks[0].psp = MCB_PSP_FREE;
            DOSMEM_umb_count = 1;
            DOSMEM_umb_initialized = TRUE;

            TRACE("DOS conventional memory initialized, %d bytes free, %d bytes UMB free.\n",
                  DOSMEM_Available(), DOSMEM_AvailableHigh());

            DOSMEM_InitSegments();

            SetEvent( hRunOnce );
            done = TRUE;
            return ret;
	}
	/* someone beat us here... */
	CloseHandle( hEvent );
    }

    /* and wait for the winner to have finished */
    WaitForSingleObject( hRunOnce, INFINITE );
    return TRUE;
}

/******************************************************************
 *		dosmem_handler
 *
 * Handler to catch access to our 1MB address space reserved for real memory
 */
static LONG WINAPI dosmem_handler(EXCEPTION_POINTERS* except)
{
    if (except->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION)
    {
        char *addr = (char *)except->ExceptionRecord->ExceptionInformation[1];
        if (addr >= DOSMEM_dosmem + DOSMEM_protect && addr < DOSMEM_dosmem + DOSMEM_SIZE)
        {
            if (DOSMEM_InitDosMemory()) return EXCEPTION_CONTINUE_EXECUTION;
        }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

/***********************************************************************
 *           DOSMEM_Init
 *
 * Create the dos memory segments, and store them into the KERNEL
 * exported values.
 */
BOOL DOSMEM_Init(void)
{
    void *addr = (void *)1;
    SIZE_T size = DOSMEM_SIZE - 1;

    if (NtAllocateVirtualMemory( GetCurrentProcess(), &addr, 0, &size,
                                 MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS ))
    {
        ERR( "Cannot allocate DOS memory\n" );
        ExitProcess(1);
    }

    if (addr <= (void *)DOSMEM_64KB)
    {
        DOSMEM_dosmem = 0;
        DOSMEM_protect = DOSMEM_64KB;
        DOSMEM_sysmem = (char *)0xf0000;  /* store sysmem in high addresses for now */
    }
    else
    {
        WARN( "First megabyte not available for DOS address space.\n" );
        DOSMEM_dosmem = addr;
        DOSMEM_protect = 0;
        DOSMEM_sysmem = DOSMEM_dosmem;
    }

    vectored_handler = AddVectoredExceptionHandler(FALSE, dosmem_handler);
    DOSMEM_0000H = GLOBAL_CreateBlock( GMEM_FIXED, DOSMEM_sysmem, DOSMEM_64KB, 0, data_segment );
    DOSMEM_BiosDataSeg = GLOBAL_CreateBlock( GMEM_FIXED, DOSMEM_sysmem + 0x400, 0x100, 0, data_segment );
    DOSMEM_BiosSysSeg = GLOBAL_CreateBlock( GMEM_FIXED, DOSMEM_dosmem + 0xf0000, DOSMEM_64KB, 0, data_segment );

    return TRUE;
}

/***********************************************************************
 *           DOSMEM_MapLinearToDos
 *
 * Linear address to the DOS address space.
 */
UINT DOSMEM_MapLinearToDos(LPVOID ptr)
{
    if (((char*)ptr >= DOSMEM_dosmem) &&
        ((char*)ptr < DOSMEM_dosmem + DOSMEM_SIZE))
          return (char *)ptr - DOSMEM_dosmem;
    return (UINT)ptr;
}


/***********************************************************************
 *           DOSMEM_MapDosToLinear
 *
 * DOS linear address to the linear address space.
 */
LPVOID DOSMEM_MapDosToLinear(UINT ptr)
{
    if (ptr < DOSMEM_SIZE) return DOSMEM_dosmem + ptr;
    return (LPVOID)ptr;
}


/***********************************************************************
 *           DOSMEM_MapRealToLinear
 *
 * Real mode DOS address into a linear pointer
 */
LPVOID DOSMEM_MapRealToLinear(DWORD x)
{
   LPVOID       lin;

   lin = DOSMEM_dosmem + HIWORD(x) * 16 + LOWORD(x);
   TRACE_(selector)("(0x%08lx) returns %p.\n", x, lin );
   return lin;
}

/***********************************************************************
 *           DOSMEM_AllocFromChain
 *
 * Allocate from one DOS MCB chain using first, best, or last fit.
 */
static LPVOID DOSMEM_AllocFromChain( MCB *root, UINT size, UINT16 *pseg, BYTE strategy )
{
    MCB *curr, *chosen = NULL, *next;
    WORD psp;

    if (!(psp = DOSVM_psp)) psp = MCB_PSP_DOS;
    if (pseg) *pseg = 0;

    size = (size + 15) >> 4;
    strategy &= 3;

    for (curr = root; curr; curr = MCB_NEXT(curr))
    {
        if (!MCB_VALID(curr))
        {
            ERR("MCB List Corrupt\n");
            MCB_DUMP(curr);
            return NULL;
        }
        if (curr->psp != MCB_PSP_FREE) continue;

        DOSMEM_Collapse(curr);
        if (curr->size < size) continue;

        if (!chosen || strategy == 0 ||
            (strategy == 1 && curr->size < chosen->size) ||
            strategy == 2)
            chosen = curr;

        if (strategy == 0) break;
    }

    if (!chosen) return NULL;

    if (strategy == 2 && chosen->size > size)
    {
        /* Last fit allocates from the high end of the selected free block. */
        next = (MCB *)((char *)chosen + ((chosen->size - size) << 4));
        next->type = chosen->type;
        next->psp = psp;
        next->size = size;
        chosen->type = MCB_TYPE_NORMAL;
        chosen->size -= size + 1;
        chosen = next;
    }
    else
    {
        if (chosen->size > size)
        {
            next = (MCB *)((char *)chosen + ((size + 1) << 4));
            next->psp = MCB_PSP_FREE;
            next->size = chosen->size - (size + 1);
            next->type = chosen->type;
            chosen->type = MCB_TYPE_NORMAL;
            chosen->size = size;
        }
        chosen->psp = psp;
    }

    if (pseg) *pseg = ((char *)chosen + 16 - DOSMEM_dosmem) >> 4;
    return (char *)chosen + 16;
}


/***********************************************************************
 *           DOSMEM_AllocBlock
 *
 * Carve a chunk of conventional DOS memory using first fit.
 */
LPVOID DOSMEM_AllocBlock(UINT size, UINT16* pseg)
{
    DOSMEM_InitDosMemory();
    TRACE("(low,%04xh)\n", size);
    return DOSMEM_AllocFromChain(DOSMEM_root_block, size, pseg, 0);
}


/***********************************************************************
 *           DOSMEM_AllocBlockStrategy
 */
LPVOID DOSMEM_AllocBlockStrategy(UINT size, UINT16 *pseg, BYTE strategy)
{
    DOSMEM_InitDosMemory();
    TRACE("(low,%04xh,strategy=%02x)\n", size, strategy);
    return DOSMEM_AllocFromChain(DOSMEM_root_block, size, pseg, strategy);
}


static BOOL DOSMEM_UMBInsert(unsigned int index, UMB_BLOCK block)
{
    if (DOSMEM_umb_count >= DOSMEM_UMB_MAX_BLOCKS || index > DOSMEM_umb_count)
        return FALSE;

    memmove(&DOSMEM_umb_blocks[index + 1], &DOSMEM_umb_blocks[index],
            (DOSMEM_umb_count - index) * sizeof(DOSMEM_umb_blocks[0]));
    DOSMEM_umb_blocks[index] = block;
    DOSMEM_umb_count++;
    return TRUE;
}

static void DOSMEM_UMBRemove(unsigned int index)
{
    if (index >= DOSMEM_umb_count) return;

    memmove(&DOSMEM_umb_blocks[index], &DOSMEM_umb_blocks[index + 1],
            (DOSMEM_umb_count - index - 1) * sizeof(DOSMEM_umb_blocks[0]));
    DOSMEM_umb_count--;
}

static void DOSMEM_UMBCollapse(unsigned int index)
{
    if (index >= DOSMEM_umb_count || DOSMEM_umb_blocks[index].psp != MCB_PSP_FREE)
        return;

    if (index && DOSMEM_umb_blocks[index - 1].psp == MCB_PSP_FREE)
    {
        DOSMEM_umb_blocks[index - 1].size += DOSMEM_umb_blocks[index].size;
        DOSMEM_UMBRemove(index);
        index--;
    }

    if (index + 1 < DOSMEM_umb_count && DOSMEM_umb_blocks[index + 1].psp == MCB_PSP_FREE)
    {
        DOSMEM_umb_blocks[index].size += DOSMEM_umb_blocks[index + 1].size;
        DOSMEM_UMBRemove(index + 1);
    }
}

/***********************************************************************
 *           DOSMEM_AllocBlockHigh
 *
 * Allocate from the upper-memory address range.  UMB bookkeeping is kept
 * outside the emulated UMA so an EMS page frame can reserve and own every
 * byte of its physical window.
 */
LPVOID DOSMEM_AllocBlockHigh(UINT size, UINT16 *pseg, BYTE strategy)
{
    unsigned int i, chosen = DOSMEM_UMB_MAX_BLOCKS;
    UINT paragraphs = (size + 15) >> 4;
    WORD psp = DOSVM_psp ? DOSVM_psp : MCB_PSP_DOS;
    UMB_BLOCK block;

    DOSMEM_InitDosMemory();
    strategy &= 3;
    if (pseg) *pseg = 0;

    TRACE("(high,%04xh,strategy=%02x)\n", size, strategy);

    for (i = 0; i < DOSMEM_umb_count; i++)
    {
        if (DOSMEM_umb_blocks[i].psp != MCB_PSP_FREE ||
            DOSMEM_umb_blocks[i].size < paragraphs)
            continue;

        if (chosen == DOSMEM_UMB_MAX_BLOCKS || strategy == 0 ||
            (strategy == 1 && DOSMEM_umb_blocks[i].size < DOSMEM_umb_blocks[chosen].size) ||
            strategy == 2)
            chosen = i;

        if (strategy == 0) break;
    }

    if (chosen == DOSMEM_UMB_MAX_BLOCKS) return NULL;

    block = DOSMEM_umb_blocks[chosen];
    if (block.size == paragraphs)
    {
        DOSMEM_umb_blocks[chosen].psp = psp;
    }
    else if (strategy == 2)
    {
        UMB_BLOCK allocated;

        DOSMEM_umb_blocks[chosen].size -= paragraphs;
        allocated.segment = DOSMEM_umb_blocks[chosen].segment + DOSMEM_umb_blocks[chosen].size;
        allocated.size = paragraphs;
        allocated.psp = psp;
        if (!DOSMEM_UMBInsert(chosen + 1, allocated))
        {
            DOSMEM_umb_blocks[chosen] = block;
            return NULL;
        }
        chosen++;
    }
    else
    {
        UMB_BLOCK remainder;

        DOSMEM_umb_blocks[chosen].size = paragraphs;
        DOSMEM_umb_blocks[chosen].psp = psp;
        remainder.segment = block.segment + paragraphs;
        remainder.size = block.size - paragraphs;
        remainder.psp = MCB_PSP_FREE;
        if (!DOSMEM_UMBInsert(chosen + 1, remainder))
        {
            DOSMEM_umb_blocks[chosen] = block;
            return NULL;
        }
    }

    if (pseg) *pseg = DOSMEM_umb_blocks[chosen].segment;
    return DOSMEM_dosmem + ((UINT)DOSMEM_umb_blocks[chosen].segment << 4);
}

BOOL DOSMEM_ReserveUMB(WORD segment, UINT paragraphs)
{
    unsigned int i;
    UINT end = (UINT)segment + paragraphs;

    DOSMEM_InitDosMemory();

    if (!paragraphs || segment < (DOSMEM_UMB_BOTTOM >> 4) ||
        end > ((DOSMEM_UMB_TOP + 1) >> 4))
        return FALSE;

    for (i = 0; i < DOSMEM_umb_count; i++)
    {
        UMB_BLOCK old = DOSMEM_umb_blocks[i];
        UINT old_end = (UINT)old.segment + old.size;
        UINT before, after;

        if (old.psp != MCB_PSP_FREE || segment < old.segment || end > old_end)
            continue;

        before = segment - old.segment;
        after = old_end - end;
        if (DOSMEM_umb_count + (before != 0) + (after != 0) - 1 > DOSMEM_UMB_MAX_BLOCKS)
            return FALSE;

        if (before)
        {
            DOSMEM_umb_blocks[i].size = before;
            if (!DOSMEM_UMBInsert(i + 1, (UMB_BLOCK){ segment, paragraphs, DOSMEM_UMB_RESERVED }))
                return FALSE;
            i++;
        }
        else
        {
            DOSMEM_umb_blocks[i].segment = segment;
            DOSMEM_umb_blocks[i].size = paragraphs;
            DOSMEM_umb_blocks[i].psp = DOSMEM_UMB_RESERVED;
        }

        if (after)
            return DOSMEM_UMBInsert(i + 1, (UMB_BLOCK){ (WORD)end, after, MCB_PSP_FREE });
        return TRUE;
    }
    return FALSE;
}

static BOOL DOSMEM_FreeBlockHigh(void *ptr)
{
    UINT dosaddr = (char *)ptr - DOSMEM_dosmem;
    WORD segment = dosaddr >> 4;
    unsigned int i;

    for (i = 0; i < DOSMEM_umb_count; i++)
    {
        if (DOSMEM_umb_blocks[i].segment != segment ||
            DOSMEM_umb_blocks[i].psp == MCB_PSP_FREE ||
            DOSMEM_umb_blocks[i].psp == DOSMEM_UMB_RESERVED)
            continue;

        DOSMEM_umb_blocks[i].psp = MCB_PSP_FREE;
        DOSMEM_UMBCollapse(i);
        return TRUE;
    }
    return FALSE;
}

/***********************************************************************
 *           DOSMEM_FreeBlock
 */
BOOL DOSMEM_FreeBlock(void* ptr)
{
    MCB* mcb;

    TRACE( "(%p)\n", ptr );

    if ((char *)ptr >= DOSMEM_dosmem + DOSMEM_UMB_BOTTOM &&
        (char *)ptr <= DOSMEM_dosmem + DOSMEM_UMB_TOP)
        return DOSMEM_FreeBlockHigh(ptr);

    mcb = (MCB*) ((char*)ptr - 16);

#ifdef __DOSMEM_DEBUG__
    DOSMEM_Available();
#endif

    if (!MCB_VALID (mcb))
    {
        ERR( "MCB invalid\n" );
        MCB_DUMP( mcb );
        return FALSE;
    }

    mcb->psp = MCB_PSP_FREE;
    DOSMEM_Collapse( mcb );
    return TRUE;
}

/***********************************************************************
 *           DOSMEM_ResizeBlock
 *
 * Resize DOS memory block in place. Returns block size or -1 on error.
 *
 * If exact is TRUE, returned value is either old or requested block
 * size. If exact is FALSE, block is expanded even if there is not
 * enough space for full requested block size.
 *
 * TODO: return also biggest block size
 */
UINT DOSMEM_ResizeBlock(void *ptr, UINT size, BOOL exact)
{
    MCB* mcb = (MCB*) ((char*)ptr - 16);
    MCB* next;

    TRACE( "(%p,%04xh,%s)\n", ptr, size, exact ? "TRUE" : "FALSE" );

    /* round up to paragraph */
    size = (size + 15) >> 4;

#ifdef __DOSMEM_DEBUG__
    DOSMEM_Available();
#endif

    if (!MCB_VALID (mcb))
    {
        ERR( "MCB invalid\n" );
        MCB_DUMP( mcb );
        return -1;
    }

    /* resize needed? */
    if (mcb->size == size)
        return size << 4;

    /* collapse free blocks */
    DOSMEM_Collapse( mcb );

    /* shrink mcb ? */
    if (mcb->size > size)
    {
        next = (MCB *) ((char*)mcb + ((size+1) << 4));
        next->type = mcb->type;
        next->psp = MCB_PSP_FREE;
        next->size = mcb->size - (size+1);
        mcb->type = MCB_TYPE_NORMAL;
        mcb->size = size;
        return size << 4;
    }

    if (!exact)
    {
        return mcb->size << 4;
    }

    return -1;
}

/***********************************************************************
 *           DOSMEM_Available
 */
static UINT DOSMEM_AvailableInChain(MCB *root)
{
    UINT available = 0;
    MCB *curr = root;

    while (curr)
    {
#ifdef __DOSMEM_DEBUG__
        MCB_DUMP(curr);
#endif
        if (!MCB_VALID(curr))
        {
            ERR("MCB List Corrupt\n");
            MCB_DUMP(curr);
            return 0;
        }
        if (curr->psp == MCB_PSP_FREE && curr->size > available)
            available = curr->size;
        curr = MCB_NEXT(curr);
    }
    return available << 4;
}

UINT DOSMEM_Available(void)
{
    UINT available;

    if (!DOSMEM_root_block) DOSMEM_InitDosMemory();
    available = DOSMEM_AvailableInChain(DOSMEM_root_block);
    TRACE("%04xh paragraphs conventional memory available\n", available >> 4);
    return available;
}

UINT DOSMEM_AvailableHigh(void)
{
    UINT available = 0;
    unsigned int i;

    if (!DOSMEM_umb_initialized) DOSMEM_InitDosMemory();

    for (i = 0; i < DOSMEM_umb_count; i++)
        if (DOSMEM_umb_blocks[i].psp == MCB_PSP_FREE &&
            DOSMEM_umb_blocks[i].size > available)
            available = DOSMEM_umb_blocks[i].size;

    TRACE("%04xh paragraphs upper memory available\n", available);
    return available << 4;
}

/******************************************************************
 *		DOSMEM_MapDosLayout
 *
 * Initialize the first MB of memory to look like a real DOS setup
 */
BOOL DOSMEM_MapDosLayout(void)
{
    static BOOL already_mapped;
    DWORD old_prot;

    if (!already_mapped)
    {
        if (DOSMEM_dosmem || !VirtualProtect( NULL, DOSMEM_SIZE, PAGE_EXECUTE_READWRITE, &old_prot ))
        {
            ERR( "Need full access to the first megabyte for DOS mode\n" );
            ExitProcess(1);
        }
        /* copy the BIOS and ISR area down */
        memcpy( DOSMEM_dosmem, DOSMEM_sysmem, 0x400 + 0x100 );
        DOSMEM_sysmem = DOSMEM_dosmem;
        SetSelectorBase( DOSMEM_0000H, 0 );
        SetSelectorBase( DOSMEM_BiosDataSeg, 0x400 );
        /* we may now need the actual interrupt stubs, and since we've just moved the
         * interrupt vector table away, we can fill the area with stubs instead... */
        DOSMEM_MakeIsrStubs();
        already_mapped = TRUE;
    }
    return TRUE;
}
