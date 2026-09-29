# ordinal exports
# Water-private WOWDEB bridge.  These are not public Windows exports.
@ cdecl -private __wine_WOWDebugPoll16(long)
@ cdecl -private __wine_WOWDebugGetRemoteBlock()

# NT VDM dispatcher entry points.  Keep them name-only so the legacy
# WOWGetDescriptor ordinal remains compatible with the existing Wine ABI.
@ stdcall W32Dispatch()
@ stdcall W32Init(long)

# Water-private bridge used by NT5 KERNEL.506 WOW16Call.
@ cdecl -private __wine_W32DispatchFrame(ptr)
@ cdecl -private __wine_W32RegisterDosInt21Handler(ptr)
@ cdecl -private __wine_W32DosInt21(ptr)

1 stdcall -import WOWGetDescriptor(long ptr) K32WOWGetDescriptor

@ stdcall -import WOWCallback16(long long) K32WOWCallback16
@ stdcall -import WOWCallback16Ex(long long long ptr ptr) K32WOWCallback16Ex
@ stdcall -import WOWDirectedYield16(long) K32WOWDirectedYield16
@ stdcall -import WOWGetVDMPointer(long long long) K32WOWGetVDMPointer
@ stdcall -import WOWGetVDMPointerFix(long long long) K32WOWGetVDMPointerFix
@ stdcall -import WOWGetVDMPointerUnfix(long) K32WOWGetVDMPointerUnfix
@ stdcall -import WOWGlobalAlloc16(long long) K32WOWGlobalAlloc16
@ stdcall -import WOWGlobalAllocLock16(long long ptr) K32WOWGlobalAllocLock16
@ stdcall -import WOWGlobalFree16(long) K32WOWGlobalFree16
@ stdcall -import WOWGlobalLock16(long) K32WOWGlobalLock16
@ stdcall -import WOWGlobalLockSize16(long ptr) K32WOWGlobalLockSize16
@ stdcall -import WOWGlobalUnlock16(long) K32WOWGlobalUnlock16
@ stdcall -import WOWGlobalUnlockFree16(long) K32WOWGlobalUnlockFree16
@ stdcall -import WOWHandle16(long long) K32WOWHandle16
@ stdcall -import WOWHandle32(long long) K32WOWHandle32
@ stdcall -import WOWYield16() K32WOWYield16
