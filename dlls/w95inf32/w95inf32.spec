# W95INF32, the Windows 95/98 32-bit INF-to-SETUPX compatibility shim.
# Historical 2004 Wine developer export survey, ordinals 1..5.
# On i386, the native export names retain a stdcall @bytes suffix; the
# extra @bytes in the .spec keeps the suffix after winebuild's decoration
# stripping. Do not apply these legacy exports to newer guest ABIs.
1 stdcall -i386 CtlSetLddPath32@8@8(long str) CtlSetLddPath32
2 stdcall -i386 GenFormStrWithoutPlaceHolders32@12@12(ptr str str) GenFormStrWithoutPlaceHolders32
# The old proposal used four arguments despite the native @20 decoration.
# Five DWORD slots preserve the 20-byte Win32 x86 stack ABI. The final
# argument's purpose is unverified; reject it when nonzero.
3 stdcall -i386 GenInstall32@20@20(str str str long long) GenInstall32
4 stdcall -i386 GetSETUPXErrorText32@12@12(long long long) GetSETUPXErrorText32
# This is a thunk-data entry, not an ordinary callable API. Its structure
# is unknown; retain an explicit unsupported stub rather than invent data.
5 stub -i386 w95thk_ThunkData32
