/*
 * Microsoft NetMeeting conferencing API definitions
 *
 * This header describes the public MSCONF.DLL version-2 ABI used by
 * late-1990s Windows conferencing clients.
 */

#ifndef __WINE_MSCONF_H
#define __WINE_MSCONF_H

#include <windef.h>
#include <winbase.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CONF_VERSION 0x0002

#define CONF_MAX_USERNAME        128
#define CONF_MAX_CONFERENCENAME  128

#define CONFERR_BASE                0x09000L
#define CONFERR_INVALID_PARAMETER   (CONFERR_BASE + 1)
#define CONFERR_INVALID_HCONF       (CONFERR_BASE + 2)
#define CONFERR_INVALID_BUFFER      (CONFERR_BASE + 3)
#define CONFERR_BUFFER_TOO_SMALL    (CONFERR_BASE + 4)
#define CONFERR_ENUM_COMPLETE       (CONFERR_BASE + 5)
#define CONFERR_INVALID_OPERATION   (CONFERR_BASE + 6)
#define CONFERR_INVALID_ADDRESS     (CONFERR_BASE + 7)
#define CONFERR_FILE_TRANSFER       (CONFERR_BASE + 10)
#define CONFERR_FILE_SEND_ABORT     (CONFERR_BASE + 11)
#define CONFERR_FILE_RECEIVE_ABORT  (CONFERR_BASE + 12)
#define CONFERR_NO_APP_SHARING      (CONFERR_BASE + 20)
#define CONFERR_NOT_SHARED          (CONFERR_BASE + 21)
#define CONFERR_NOT_SHAREABLE       (CONFERR_BASE + 22)
#define CONFERR_ALREADY_SHARED      (CONFERR_BASE + 23)
#define CONFERR_OUT_OF_MEMORY       ERROR_NOT_ENOUGH_MEMORY
#define CONFERR_FILE_NOT_FOUND      ERROR_FILE_NOT_FOUND
#define CONFERR_PATH_NOT_FOUND      ERROR_PATH_NOT_FOUND
#define CONFERR_ACCESS_DENIED       ERROR_ACCESS_DENIED
#define CONFERR_RECEIVE_DIR         ERROR_DISK_FULL
#define CONFERR_NOT_IMPLEMENTED     ERROR_CALL_NOT_IMPLEMENTED
#define CONFERR_INVALID_HWND        ERROR_INVALID_WINDOW_HANDLE
#define CONFERR_INTERNAL            (CONFERR_BASE + 99)
#define CONFERR_SUCCESS             0

typedef DWORD CONFERR;
typedef HANDLE HCONF;
typedef HANDLE HCONFNOTIFY;
typedef LONG (CALLBACK *CONFNOTIFYPROC)(HCONF, DWORD, DWORD, LPVOID, LPVOID, DWORD);

#include <pshpack4.h>

#ifndef ANSI_ONLY
typedef struct _CONFADDRW
{
    DWORD dwSize;
    DWORD dwAddrType;
    union
    {
        DWORD dwIp;
        LPCWSTR psz;
    };
} CONFADDRW, *LPCONFADDRW;
#endif

#ifndef UNICODE_ONLY
typedef struct _CONFADDRA
{
    DWORD dwSize;
    DWORD dwAddrType;
    union
    {
        DWORD dwIp;
        LPCSTR psz;
    };
} CONFADDRA, *LPCONFADDRA;
#endif

#ifdef UNICODE
typedef CONFADDRW CONFADDR;
typedef LPCONFADDRW LPCONFADDR;
#else
typedef CONFADDRA CONFADDR;
typedef LPCONFADDRA LPCONFADDR;
#endif

#define CONF_ADDR_UNKNOWN      0x0000
#define CONF_ADDR_IP           0x0001
#define CONF_ADDR_MACHINENAME  0x0002
#define CONF_ADDR_PSTN         0x0003

typedef struct tagConfDest
{
    DWORD dwSize;
    DWORD dwFlags;
    DWORD dwUserId;
    DWORD dwReserved;
    GUID guid;
} CONFDEST, *LPCONFDEST;

#define CONF_DF_BROADCAST           0x0100
#define CONF_DF_PRIVATE             0x0200
#define CONF_DF_DATA_SEGMENT_BEGIN  0x0400
#define CONF_DF_DATA_SEGMENT_END    0x0800

#ifndef ANSI_ONLY
typedef struct _CONFINFOW
{
    DWORD dwSize;
    HCONF hConf;
    DWORD dwMediaType;
    DWORD dwState;
    DWORD cUsers;
    DWORD dwGCCID;
    WCHAR szConferenceName[CONF_MAX_CONFERENCENAME];
} CONFINFOW, *LPCONFINFOW;
#endif

#ifndef UNICODE_ONLY
typedef struct _CONFINFOA
{
    DWORD dwSize;
    HCONF hConf;
    DWORD dwMediaType;
    DWORD dwState;
    DWORD cUsers;
    DWORD dwGCCID;
    CHAR szConferenceName[CONF_MAX_CONFERENCENAME];
} CONFINFOA, *LPCONFINFOA;
#endif

#ifdef UNICODE
typedef CONFINFOW CONFINFO;
typedef LPCONFINFOW LPCONFINFO;
#else
typedef CONFINFOA CONFINFO;
typedef LPCONFINFOA LPCONFINFO;
#endif

#define CONF_MT_DATA   0x0001
#define CONF_MT_AUDIO  0x0002
#define CONF_MT_VIDEO  0x0004
#define CONF_MT_ALL    0x00ff

#define CONF_CS_INVALID       0x0000
#define CONF_CS_INITIALIZING  0x0001
#define CONF_CS_ACTIVE        0x0002
#define CONF_CS_STOPPING      0x0003

#ifndef ANSI_ONLY
typedef struct _CONFUSERINFOW
{
    DWORD dwSize;
    DWORD dwUserId;
    DWORD dwFlags;
    DWORD dwReserved;
    WCHAR szUserName[CONF_MAX_USERNAME];
} CONFUSERINFOW, *LPCONFUSERINFOW;
#endif

#ifndef UNICODE_ONLY
typedef struct _CONFUSERINFOA
{
    DWORD dwSize;
    DWORD dwUserId;
    DWORD dwFlags;
    DWORD dwReserved;
    CHAR szUserName[CONF_MAX_USERNAME];
} CONFUSERINFOA, *LPCONFUSERINFOA;
#endif

#ifdef UNICODE
typedef CONFUSERINFOW CONFUSERINFO;
typedef LPCONFUSERINFOW LPCONFUSERINFO;
#else
typedef CONFUSERINFOA CONFUSERINFO;
typedef LPCONFUSERINFOA LPCONFUSERINFO;
#endif

#define CONF_UF_DATA   0x00000001
#define CONF_UF_AUDIO  0x00000002
#define CONF_UF_VIDEO  0x00000004
#define CONF_UF_LOCAL  0x00010000

#ifndef ANSI_ONLY
typedef struct _CONFRECDIRW
{
    DWORD dwSize;
    WCHAR szRecDir[MAX_PATH];
} CONFRECDIRW, *LPCONFRECDIRW;
#endif

#ifndef UNICODE_ONLY
typedef struct _CONFRECDIRA
{
    DWORD dwSize;
    CHAR szRecDir[MAX_PATH];
} CONFRECDIRA, *LPCONFRECDIRA;
#endif

#ifdef UNICODE
typedef CONFRECDIRW CONFRECDIR;
typedef LPCONFRECDIRW LPCONFRECDIR;
#else
typedef CONFRECDIRA CONFRECDIR;
typedef LPCONFRECDIRA LPCONFRECDIR;
#endif

typedef struct _CONFNOTIFY
{
    DWORD dwSize;
    DWORD dwUser;
    DWORD dwFlags;
    GUID guid;
    CONFNOTIFYPROC pfnNotifyProc;
} CONFNOTIFY, *LPCONFNOTIFY;

#ifndef ANSI_ONLY
typedef struct _CONFGUIDW
{
    DWORD dwSize;
    GUID guid;
    LPCWSTR pszApplication;
    LPCWSTR pszCommandLine;
    LPCWSTR pszDirectory;
} CONFGUIDW, *LPCONFGUIDW;
#endif

#ifndef UNICODE_ONLY
typedef struct _CONFGUIDA
{
    DWORD dwSize;
    GUID guid;
    LPCSTR pszApplication;
    LPCSTR pszCommandLine;
    LPCSTR pszDirectory;
} CONFGUIDA, *LPCONFGUIDA;
#endif

#ifdef UNICODE
typedef CONFGUIDW CONFGUID;
typedef LPCONFGUIDW LPCONFGUID;
#else
typedef CONFGUIDA CONFGUID;
typedef LPCONFGUIDA LPCONFGUID;
#endif

#ifndef ANSI_ONLY
typedef struct _CONFFILEINFOW
{
    DWORD dwSize;
    DWORD dwFileId;
    DWORD dwReserved1;
    DWORD dwFileSize;
    DWORD dwReserved2;
    DWORD dwBytesTransferred;
    DWORD dwFileAttributes;
    FILETIME ftCreationTime;
    FILETIME ftLastAccessTime;
    FILETIME ftLastWriteTime;
    WCHAR szFileNameSrc[MAX_PATH];
    WCHAR szFileNameDest[MAX_PATH];
} CONFFILEINFOW, *LPCONFFILEINFOW;
#endif

#ifndef UNICODE_ONLY
typedef struct _CONFFILEINFOA
{
    DWORD dwSize;
    DWORD dwFileId;
    DWORD dwReserved1;
    DWORD dwFileSize;
    DWORD dwReserved2;
    DWORD dwBytesTransferred;
    DWORD dwFileAttributes;
    FILETIME ftCreationTime;
    FILETIME ftLastAccessTime;
    FILETIME ftLastWriteTime;
    CHAR szFileNameSrc[MAX_PATH];
    CHAR szFileNameDest[MAX_PATH];
} CONFFILEINFOA, *LPCONFFILEINFOA;
#endif

#ifdef UNICODE
typedef CONFFILEINFOW CONFFILEINFO;
typedef LPCONFFILEINFOW LPCONFFILEINFO;
#else
typedef CONFFILEINFOA CONFFILEINFO;
typedef LPCONFFILEINFOA LPCONFFILEINFO;
#endif

#include <poppack.h>

#define CONF_GET_CONF      0x0001
#define CONF_ENUM_CONF     0x0002
#define CONF_GET_USER      0x0011
#define CONF_ENUM_USER     0x0012
#define CONF_ENUM_PEER     0x0018
#define CONF_GET_RECDIR    0x0020
#define CONF_GET_FILEINFO  0x0021

#define CONF_SET_RECDIR  0x1020
#define CONF_SET_GUID    0x1041

#define CONF_SF_NOWAIT      0x0001
#define CONF_SF_NOUI        0x0002
#define CONF_SF_NOCOMPRESS  0x0004

#define CONF_SW_SHARE       0x0001
#define CONF_SW_UNSHARE     0x0002
#define CONF_SW_SHAREABLE   0x0003
#define CONF_SW_IS_SHARED   0x0004

#define CONFN_CONFERENCE_INIT       0x0001
#define CONFN_CONFERENCE_START      0x0002
#define CONFN_CONFERENCE_STOP       0x0003
#define CONFN_CONFERENCE_ERROR      0x0004
#define CONFN_USER_ADDED            0x0011
#define CONFN_USER_REMOVED          0x0012
#define CONFN_USER_UPDATE           0x0013
#define CONFN_PEER_ADDED            0x0021
#define CONFN_PEER_REMOVED          0x0022
#define CONFN_WINDOW_SHARED         0x0041
#define CONFN_WINDOW_UNSHARED       0x0042
#define CONFN_DATA_SENT             0x0101
#define CONFN_DATA_RECEIVED         0x0102
#define CONFN_FILESEND_START        0x0111
#define CONFN_FILESEND_PROGRESS     0x0112
#define CONFN_FILESEND_COMPLETE     0x0113
#define CONFN_FILESEND_ERROR        0x0114
#define CONFN_FILERECEIVE_START     0x0121
#define CONFN_FILERECEIVE_PROGRESS  0x0122
#define CONFN_FILERECEIVE_COMPLETE  0x0123
#define CONFN_FILERECEIVE_ERROR     0x0124

#ifndef ANSI_ONLY
DWORD WINAPI ConferenceConnectW(HCONF *, LPCONFADDRW, LPCONFINFOW, LPCONFNOTIFY);
DWORD WINAPI ConferenceSendFileW(HCONF, LPCONFDEST, LPCWSTR, DWORD);
DWORD WINAPI ConferenceGetInfoW(HCONF, DWORD, LPVOID);
DWORD WINAPI ConferenceSetInfoW(HCONF, DWORD, LPVOID);
#endif

#ifndef UNICODE_ONLY
DWORD WINAPI ConferenceConnectA(HCONF *, LPCONFADDRA, LPCONFINFOA, LPCONFNOTIFY);
DWORD WINAPI ConferenceSendFileA(HCONF, LPCONFDEST, LPCSTR, DWORD);
DWORD WINAPI ConferenceGetInfoA(HCONF, DWORD, LPVOID);
DWORD WINAPI ConferenceSetInfoA(HCONF, DWORD, LPVOID);
#endif

DWORD WINAPI ConferenceListen(DWORD);
DWORD WINAPI ConferenceDisconnect(HCONF);
DWORD WINAPI ConferenceSetNotify(HCONF, LPCONFNOTIFY, HCONFNOTIFY *);
DWORD WINAPI ConferenceRemoveNotify(HCONF, HCONFNOTIFY);
DWORD WINAPI ConferenceCancelTransfer(HCONF, DWORD);
DWORD WINAPI ConferenceSendData(HCONF, LPCONFDEST, LPVOID, DWORD, DWORD);
DWORD WINAPI ConferenceLaunchRemote(HCONF, LPCONFDEST, DWORD);
DWORD WINAPI ConferenceShareWindow(HCONF, HWND, DWORD);

#ifdef UNICODE
#define ConferenceConnect  ConferenceConnectW
#define ConferenceSendFile ConferenceSendFileW
#define ConferenceGetInfo  ConferenceGetInfoW
#define ConferenceSetInfo  ConferenceSetInfoW
#else
#define ConferenceConnect  ConferenceConnectA
#define ConferenceSendFile ConferenceSendFileA
#define ConferenceGetInfo  ConferenceGetInfoA
#define ConferenceSetInfo  ConferenceSetInfoA
#endif

#ifdef __cplusplus
}
#endif

#endif
