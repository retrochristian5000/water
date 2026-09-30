/*
 * NTIO.SYS MS-DOS emulation BIOS services
 *
 * Copyright 2002 Jukka Heinonen
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * Water keeps the generic Win16 interrupt-routing machinery in KRNL386 while
 * NTIO owns BIOS service implementations for the NT VDM line. KRNL386 still
 * compiles this source through a temporary compatibility bridge until the
 * DOSX/WIN386 BIOS providers are split from the shared Win16 backend.
 */

#include <stdio.h>

#include "windef.h"
#include "winbase.h"
#include "wincon.h"
#include "winuser.h"
#include "../../dlls/krnl386.exe16/dosexe.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(int);

#define BIN_TO_BCD(x) ((x%10) + ((x/10)<<4))

/**********************************************************************
 *          DOSVM_GetKeyboardEvent
 *
 * Retrieve a BIOS-style keyboard event from the current console.
 * Non-key and modifier-only events are consumed because they do not
 * enter the BIOS keyboard buffer.
 */
static BOOL DOSVM_GetKeyboardEvent( KEY_EVENT_RECORD *key, BOOL remove )
{
    INPUT_RECORD record;
    HANDLE input = GetStdHandle( STD_INPUT_HANDLE );
    DWORD count;

    if (!input || input == INVALID_HANDLE_VALUE) return FALSE;

    for (;;)
    {
        if (remove)
        {
            if (!ReadConsoleInputA( input, &record, 1, &count ) || !count) return FALSE;
        }
        else
        {
            if (!PeekConsoleInputA( input, &record, 1, &count ) || !count) return FALSE;
        }

        if (record.EventType == KEY_EVENT && record.Event.KeyEvent.bKeyDown)
        {
            WORD vk = record.Event.KeyEvent.wVirtualKeyCode;

            if (vk != VK_SHIFT && vk != VK_CONTROL && vk != VK_MENU &&
                vk != VK_CAPITAL && vk != VK_NUMLOCK && vk != VK_SCROLL &&
                record.Event.KeyEvent.wVirtualScanCode)
            {
                *key = record.Event.KeyEvent;
                return TRUE;
            }
        }

        if (!remove && (!ReadConsoleInputA( input, &record, 1, &count ) || !count))
            return FALSE;
    }
}


/**********************************************************************
 *          DOSVM_Int16Handler
 *
 * Basic BIOS keyboard services used by DOS command-line tools such as
 * DOSKEY.COM.  Enhanced-keyboard functions 10h/11h share the same
 * behavior here because KEY_EVENT_RECORD already carries scan codes.
 */
void WINAPI DOSVM_Int16Handler( I386_CONTEXT *context )
{
    KEY_EVENT_RECORD key;
    BYTE flags = 0;

    switch (AH_reg(context))
    {
    case 0x00:  /* read keystroke */
    case 0x10:  /* enhanced keyboard - read keystroke */
        if (DOSVM_GetKeyboardEvent( &key, TRUE ))
            SET_AX( context, MAKEWORD( (BYTE)key.uChar.AsciiChar,
                                      (BYTE)key.wVirtualScanCode ) );
        break;

    case 0x01:  /* check for keystroke */
    case 0x11:  /* enhanced keyboard - check for keystroke */
        if (DOSVM_GetKeyboardEvent( &key, FALSE ))
        {
            SET_AX( context, MAKEWORD( (BYTE)key.uChar.AsciiChar,
                                      (BYTE)key.wVirtualScanCode ) );
            RESET_ZFLAG( context );
        }
        else
            SET_ZFLAG( context );
        break;

    case 0x02:  /* get shift flags */
        if (GetKeyState( VK_RSHIFT ) & 0x8000) flags |= 0x01;
        if (GetKeyState( VK_LSHIFT ) & 0x8000) flags |= 0x02;
        if (GetKeyState( VK_CONTROL ) & 0x8000) flags |= 0x04;
        if (GetKeyState( VK_MENU ) & 0x8000) flags |= 0x08;
        if (GetKeyState( VK_SCROLL ) & 0x0001) flags |= 0x10;
        if (GetKeyState( VK_NUMLOCK ) & 0x0001) flags |= 0x20;
        if (GetKeyState( VK_CAPITAL ) & 0x0001) flags |= 0x40;
        if (GetKeyState( VK_INSERT ) & 0x0001) flags |= 0x80;
        SET_AL( context, flags );
        break;

    default:
        FIXME( "INT 16h function %02x not implemented\n", AH_reg(context) );
        break;
    }
}


/**********************************************************************
 *	    DOSVM_Int11Handler
 *
 * Handler for int 11h (get equipment list).
 *
 *
 * Borrowed from Ralph Brown's interrupt lists:
 *
 *   bits 15-14: number of parallel devices
 *   bit     13: [Conv] Internal modem
 *   bit     12: reserved
 *   bits 11- 9: number of serial devices
 *   bit      8: reserved
 *   bits  7- 6: number of diskette drives minus one
 *   bits  5- 4: Initial video mode:
 *                 00b = EGA,VGA,PGA
 *                 01b = 40 x 25 color
 *                 10b = 80 x 25 color
 *                 11b = 80 x 25 mono
 *   bit      3: reserved
 *   bit      2: [PS] =1 if pointing device
 *               [non-PS] reserved
 *   bit      1: =1 if math co-processor
 *   bit      0: =1 if diskette available for boot
 *
 *
 * Currently the only of these bits correctly set are:
 *
 *   bits 15-14   } Added by William Owen Smith,
 *   bits 11-9    } wos@dcs.warwick.ac.uk
 *   bits 7-6
 *   bit  2       (always set)  ( bit 2 = 4 )
 *   bit  1       } Robert 'Admiral' Coeyman
 *                  All *nix systems either have a math processor or
 *		     emulate one.
 */
void WINAPI DOSVM_Int11Handler( I386_CONTEXT *context )
{
    int diskdrives = 0;
    int parallelports = 0;
    int serialports = 0;
    int x;

    if (GetDriveTypeA("A:\\") == DRIVE_REMOVABLE) diskdrives++;
    if (GetDriveTypeA("B:\\") == DRIVE_REMOVABLE) diskdrives++;
    if (diskdrives) diskdrives--;

    for (x=0; x < 9; x++)
    {
        HANDLE handle;
        char file[10];

        /* serial port name */
        sprintf( file, "\\\\.\\COM%d", x+1 );
        handle = CreateFileA( file, 0, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, 0 );
        if (handle != INVALID_HANDLE_VALUE)
        {
            CloseHandle( handle );
            serialports++;
        }

        sprintf( file, "\\\\.\\LPT%d", x+1 );
        handle = CreateFileA( file, 0, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, 0 );
        if (handle != INVALID_HANDLE_VALUE)
        {
            CloseHandle( handle );
            parallelports++;
        }
    }

    if (serialports > 7) /* 3 bits -- maximum value = 7 */
        serialports = 7;

    if (parallelports > 3) /* 2 bits -- maximum value = 3 */
        parallelports = 3;

    SET_AX( context,
            (diskdrives << 6) | (serialports << 9) | (parallelports << 14) | 0x06 );
}


/**********************************************************************
 *         DOSVM_Int12Handler
 *
 * Handler for int 12h (get memory size).
 */
void WINAPI DOSVM_Int12Handler( I386_CONTEXT *context )
{
    SET_AX( context, 640 );
}


/**********************************************************************
 *          DOSVM_Int17Handler
 *
 * Handler for int 17h (printer - output character).
 */
void WINAPI DOSVM_Int17Handler( I386_CONTEXT *context )
{
    switch( AH_reg(context) )
    {
       case 0x00:/* Send character*/
            FIXME("Send character not supported yet\n");
            SET_AH( context, 0x00 );/*Timeout*/
            break;
        case 0x01:              /* PRINTER - INITIALIZE */
            FIXME("Initialize Printer - Not Supported\n");
            SET_AH( context, 0x30 ); /* selected | out of paper */
            break;
        case 0x02:              /* PRINTER - GET STATUS */
            FIXME("Get Printer Status - Not Supported\n");
            break;
        default:
            SET_AH( context, 0 ); /* time out */
            INT_BARF( context, 0x17 );
    }
}


/**********************************************************************
 *          DOSVM_Int19Handler
 *
 * Handler for int 19h (Reboot).
 */
void WINAPI DOSVM_Int19Handler( I386_CONTEXT *context )
{
    TRACE( "Attempted Reboot\n" );
    ExitProcess(0);
}


/**********************************************************************
 *         DOSVM_Int1aHandler
 *
 * Handler for int 1ah.
 */
void WINAPI DOSVM_Int1aHandler( I386_CONTEXT *context )
{
    switch(AH_reg(context))
    {
    case 0x00: /* GET SYSTEM TIME */
        {
            BIOSDATA *data = DOSVM_BiosData();

            DOSVM_start_bios_timer();
            SET_CX( context, HIWORD(data->Ticks) );
            SET_DX( context, LOWORD(data->Ticks) );
            SET_AL( context, 0 ); /* FIXME: midnight flag is unsupported */
            TRACE( "GET SYSTEM TIME - ticks=%ld\n", data->Ticks );
        }
        break;

    case 0x01: /* SET SYSTEM TIME */
        FIXME( "SET SYSTEM TIME - not allowed\n" );
        break;

    case 0x02: /* GET REAL-TIME CLOCK TIME */
        TRACE( "GET REAL-TIME CLOCK TIME\n" );
        {
            SYSTEMTIME systime;
            GetLocalTime( &systime );
            SET_CH( context, BIN_TO_BCD(systime.wHour) );
            SET_CL( context, BIN_TO_BCD(systime.wMinute) );
            SET_DH( context, BIN_TO_BCD(systime.wSecond) );
            SET_DL( context, 0 ); /* FIXME: assume no daylight saving */
            RESET_CFLAG(context);
        }
        break;

    case 0x03: /* SET REAL-TIME CLOCK TIME */
        FIXME( "SET REAL-TIME CLOCK TIME - not allowed\n" );
        break;

    case 0x04: /* GET REAL-TIME CLOCK DATE */
        TRACE( "GET REAL-TIME CLOCK DATE\n" );
        {
            SYSTEMTIME systime;
            GetLocalTime( &systime );
            SET_CH( context, BIN_TO_BCD(systime.wYear / 100) );
            SET_CL( context, BIN_TO_BCD(systime.wYear % 100) );
            SET_DH( context, BIN_TO_BCD(systime.wMonth) );
            SET_DL( context, BIN_TO_BCD(systime.wDay) );
            RESET_CFLAG(context);
        }
        break;

    case 0x05: /* SET REAL-TIME CLOCK DATE */
        FIXME( "SET REAL-TIME CLOCK DATE - not allowed\n" );
        break;

    case 0x06: /* SET ALARM */
        FIXME( "SET ALARM - unimplemented\n" );
        break;

    case 0x07: /* CANCEL ALARM */
        FIXME( "CANCEL ALARM - unimplemented\n" );
        break;

    case 0x08: /* SET RTC ACTIVATED POWER ON MODE */
    case 0x09: /* READ RTC ALARM TIME AND STATUS */
    case 0x0a: /* READ SYSTEM-TIMER DAY COUNTER */
    case 0x0b: /* SET SYSTEM-TIMER DAY COUNTER */
    case 0x0c: /* SET RTC DATE/TIME ACTIVATED POWER-ON MODE */
    case 0x0d: /* RESET RTC DATE/TIME ACTIVATED POWER-ON MODE */
    case 0x0e: /* GET RTC DATE/TIME ALARM AND STATUS */
    case 0x0f: /* INITIALIZE REAL-TIME CLOCK */
        INT_BARF( context, 0x1a );
        break;

    case 0xb0:
        if (CX_reg(context) == 0x4d52 &&
            DX_reg(context) == 0x4349 &&
            AL_reg(context) == 0x01)
        {
            /*
             * Microsoft Real-Time Compression Interface (MRCI).
             * Ignoring this call indicates MRCI is not supported.
             */
            TRACE( "Microsoft Real-Time Compression Interface - not supported\n" );
        }
        else
        {
            INT_BARF(context, 0x1a);
        }
        break;

    default:
        INT_BARF( context, 0x1a );
    }
}


