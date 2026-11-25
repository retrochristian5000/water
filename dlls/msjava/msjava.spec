# Named exports and ordinals from the published x86 5.00.3810 export listing.
# Other builds have not been verified.
147 stdcall -private DllCanUnloadNow()
148 stdcall -private DllGetClassObject(ptr ptr ptr)
241 stdcall JNI_CreateJavaVM(ptr ptr ptr)
242 stdcall JNI_GetCreatedJavaVMs(ptr long ptr)
243 stdcall JNI_GetDefaultJavaVMInitArgs(ptr)
