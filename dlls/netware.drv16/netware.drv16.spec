# Windows 3.x WinNet network-driver entry points.
# The DDK assigns ordinals 1-20 to the base interface.
 1 pascal -ret16 WNetOpenJob(ptr ptr word ptr) NETWARE_WNetOpenJob
 2 pascal -ret16 WNetCloseJob(word ptr ptr) NETWARE_WNetCloseJob
 3 pascal -ret16 WNetAbortJob(ptr word) NETWARE_WNetAbortJob
 4 pascal -ret16 WNetHoldJob(ptr word) NETWARE_WNetHoldJob
 5 pascal -ret16 WNetReleaseJob(ptr word) NETWARE_WNetReleaseJob
 6 pascal -ret16 WNetCancelJob(ptr word) NETWARE_WNetCancelJob
 7 pascal -ret16 WNetSetJobCopies(ptr word word) NETWARE_WNetSetJobCopies
 8 pascal -ret16 WNetWatchQueue(word ptr ptr word) NETWARE_WNetWatchQueue
 9 pascal -ret16 WNetUnwatchQueue(str) NETWARE_WNetUnwatchQueue
10 pascal -ret16 WNetLockQueueData(ptr ptr ptr) NETWARE_WNetLockQueueData
11 pascal -ret16 WNetUnlockQueueData(ptr) NETWARE_WNetUnlockQueueData
12 pascal -ret16 WNetGetConnection(ptr ptr ptr) NETWARE_WNetGetConnection
13 pascal -ret16 WNetGetCaps(word) NETWARE_WNetGetCaps
14 pascal -ret16 WNetDeviceMode(word) NETWARE_WNetDeviceMode
15 pascal -ret16 WNetBrowseDialog(word word ptr) NETWARE_WNetBrowseDialog
16 pascal -ret16 WNetGetUser(ptr ptr) NETWARE_WNetGetUser
17 pascal -ret16 WNetAddConnection(str str str) NETWARE_WNetAddConnection
18 pascal -ret16 WNetCancelConnection(str word) NETWARE_WNetCancelConnection
19 pascal -ret16 WNetGetError(ptr) NETWARE_WNetGetError
20 pascal -ret16 WNetGetErrorText(word ptr ptr) NETWARE_WNetGetErrorText

# The DDK specifies these as ordinals 21h and 22h.
33 pascal -ret16 Enable() NETWARE_Enable
34 pascal -ret16 Disable() NETWARE_Disable

# WEP is required by name. Keep its arbitrary ordinal in the network
# driver's user-reserved range rather than occupying a WinNet ordinal.
500 pascal -ret16 WEP(word) NETWARE_WEP
