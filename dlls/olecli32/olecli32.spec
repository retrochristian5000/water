   1 stub WEP
   2 stub OleDelete
   3 stdcall OleSaveToStream(ptr ptr) OLECLI32_OleSaveToStream
   4 stdcall OleLoadFromStream(ptr str ptr long str ptr) OLECLI32_OleLoadFromStream
   6 stub OleClone
   7 stub OleCopyFromLink
   8 stub OleEqual
   9 stdcall OleQueryLinkFromClip(str long long)
  10 stdcall OleQueryCreateFromClip(str long long)
  11 stdcall OleCreateLinkFromClip(str ptr long str ptr long long)
  12 stdcall OleCreateFromClip(str ptr long str ptr long long)
  13 stub OleCopyToClipboard
  14 stdcall OleQueryType(ptr ptr)
  15 stdcall OleSetHostNames(ptr str str)
  16 stub OleSetTargetDevice
  17 stub OleSetBounds
  18 stub OleQueryBounds
  19 stub OleDraw
  20 stub OleQueryOpen
  21 stub OleActivate
  22 stub OleUpdate
  23 stub OleReconnect
  24 stub OleGetLinkUpdateOptions
  25 stub OleSetLinkUpdateOptions
  26 stub OleEnumFormats
  27 stub OleClose
  28 stub OleGetData
  29 stub OleSetData
  30 stub OleQueryProtocol
  31 stub OleQueryOutOfDate
  32 stub OleObjectConvert
  33 stub OleCreateFromTemplate
  34 stdcall OleCreate(str ptr str long str ptr long long) OLECLI32_OleCreate
  35 stub OleQueryReleaseStatus
  36 stub OleQueryReleaseError
  37 stub OleQueryReleaseMethod
  38 stdcall OleCreateFromFile(str ptr str str long str ptr long long) OLECLI32_OleCreateFromFile
  39 stub OleCreateLinkFromFile
  40 stub OleRelease
  41 stdcall OleRegisterClientDoc(str str long ptr)
  42 stdcall OleRevokeClientDoc(long)
  43 stdcall OleRenameClientDoc(long str)
  44 stub OleRevertClientDoc
  45 stdcall OleSavedClientDoc(long)
  46 stub OleRename
  47 stub OleEnumObjects
  48 stub OleQueryName
  49 stub OleSetColorScheme
  50 stub OleRequestData
  54 stub OleLockServer
  55 stub OleUnlockServer
  56 stub OleQuerySize
  57 stub OleExecute
  58 stub OleCreateInvisible
  59 stub OleQueryClientVersion
  60 stdcall OleIsDcMeta(long)
 110 stdcall DefLoadFromStream(ptr str ptr long str ptr long long long)
 111 stdcall DefCreateFromClip(str ptr long str ptr long long long)
 112 stdcall DefCreateLinkFromClip(str ptr long str ptr long long)
 113 stdcall DefCreateFromTemplate(str ptr str long str ptr long long)
 114 stdcall DefCreate(str ptr str long str ptr long long)
 115 stdcall DefCreateFromFile(str ptr str str long str ptr long long)
 116 stdcall DefCreateLinkFromFile(str ptr str str str long str ptr long long)
 117 stdcall DefCreateInvisible(str ptr str long str ptr long long long)
