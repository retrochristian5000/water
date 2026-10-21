/*
 * Baseline ISAPI extension interface supported by IIS 5.0.
 *
 * These declarations describe the ISAPI extension ABI, not the
 * HTTP Server API (httpapi.dll / http.sys) introduced after Windows 2000.
 * A server must separately implement request dispatch and the callbacks.
 *
 * Definitions are based on the public ISAPI interface; see the
 * public-domain mingw-w64 httpext.h and Microsoft's IIS SDK reference.
 */

#ifndef __WINE_HTTPEXT_H
#define __WINE_HTTPEXT_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ISAPI 2.0 is the portable baseline, not an IIS product-version claim. */
#define HSE_VERSION_MAJOR 2
#define HSE_VERSION_MINOR 0
#define HSE_VERSION MAKELONG(HSE_VERSION_MINOR, HSE_VERSION_MAJOR)

#define HSE_LOG_BUFFER_LEN 80
#define HSE_MAX_EXT_DLL_NAME_LEN 256

#define HSE_STATUS_SUCCESS 1
#define HSE_STATUS_SUCCESS_AND_KEEP_CONN 2
#define HSE_STATUS_PENDING 3
#define HSE_STATUS_ERROR 4

#define HSE_REQ_BASE 0
#define HSE_REQ_SEND_URL_REDIRECT_RESP (HSE_REQ_BASE + 1)
#define HSE_REQ_SEND_URL (HSE_REQ_BASE + 2)
#define HSE_REQ_SEND_RESPONSE_HEADER (HSE_REQ_BASE + 3)
#define HSE_REQ_DONE_WITH_SESSION (HSE_REQ_BASE + 4)
#define HSE_REQ_END_RESERVED 1000
#define HSE_REQ_MAP_URL_TO_PATH (HSE_REQ_END_RESERVED + 1)
#define HSE_REQ_GET_SSPI_INFO (HSE_REQ_END_RESERVED + 2)
#define HSE_APPEND_LOG_PARAMETER (HSE_REQ_END_RESERVED + 3)
#define HSE_REQ_IO_COMPLETION (HSE_REQ_END_RESERVED + 5)
#define HSE_REQ_TRANSMIT_FILE (HSE_REQ_END_RESERVED + 6)
#define HSE_REQ_IS_KEEP_CONN (HSE_REQ_END_RESERVED + 8)
#define HSE_REQ_ASYNC_READ_CLIENT (HSE_REQ_END_RESERVED + 10)
#define HSE_REQ_GET_IMPERSONATION_TOKEN (HSE_REQ_END_RESERVED + 11)
#define HSE_REQ_MAP_URL_TO_PATH_EX (HSE_REQ_END_RESERVED + 12)
#define HSE_REQ_SEND_RESPONSE_HEADER_EX (HSE_REQ_END_RESERVED + 16)

#define HSE_TERM_ADVISORY_UNLOAD 0x00000001
#define HSE_TERM_MUST_UNLOAD 0x00000002
#define HSE_IO_SYNC 0x00000001
#define HSE_IO_ASYNC 0x00000002
#define HSE_IO_DISCONNECT_AFTER_SEND 0x00000004
#define HSE_IO_SEND_HEADERS 0x00000008

typedef LPVOID HCONN;

typedef struct _HSE_VERSION_INFO
{
    DWORD dwExtensionVersion;
    CHAR lpszExtensionDesc[HSE_MAX_EXT_DLL_NAME_LEN];
} HSE_VERSION_INFO, *LPHSE_VERSION_INFO;

typedef struct _EXTENSION_CONTROL_BLOCK
{
    DWORD cbSize;
    DWORD dwVersion;
    HCONN ConnID;
    DWORD dwHttpStatusCode;
    CHAR lpszLogData[HSE_LOG_BUFFER_LEN];
    LPSTR lpszMethod;
    LPSTR lpszQueryString;
    LPSTR lpszPathInfo;
    LPSTR lpszPathTranslated;
    DWORD cbTotalBytes;
    DWORD cbAvailable;
    LPBYTE lpbData;
    LPSTR lpszContentType;
    BOOL (WINAPI *GetServerVariable)(HCONN, LPSTR, LPVOID, LPDWORD);
    BOOL (WINAPI *WriteClient)(HCONN, LPVOID, LPDWORD, DWORD);
    BOOL (WINAPI *ReadClient)(HCONN, LPVOID, LPDWORD);
    BOOL (WINAPI *ServerSupportFunction)(HCONN, DWORD, LPVOID, LPDWORD, LPDWORD);
} EXTENSION_CONTROL_BLOCK, *LPEXTENSION_CONTROL_BLOCK;

typedef void (WINAPI *PFN_HSE_IO_COMPLETION)(EXTENSION_CONTROL_BLOCK *, PVOID, DWORD, DWORD);

typedef struct _HSE_TF_INFO
{
    PFN_HSE_IO_COMPLETION pfnHseIO;
    PVOID pContext;
    HANDLE hFile;
    LPCSTR pszStatusCode;
    DWORD BytesToWrite;
    DWORD Offset;
    PVOID pHead;
    DWORD HeadLength;
    PVOID pTail;
    DWORD TailLength;
    DWORD dwFlags;
} HSE_TF_INFO, *LPHSE_TF_INFO;

typedef struct _HSE_SEND_HEADER_EX_INFO
{
    LPCSTR pszStatus;
    LPCSTR pszHeader;
    DWORD cchStatus;
    DWORD cchHeader;
    BOOL fKeepConn;
} HSE_SEND_HEADER_EX_INFO, *LPHSE_SEND_HEADER_EX_INFO;

typedef BOOL (WINAPI *PFN_GETEXTENSIONVERSION)(HSE_VERSION_INFO *);
typedef DWORD (WINAPI *PFN_HTTPEXTENSIONPROC)(EXTENSION_CONTROL_BLOCK *);
typedef BOOL (WINAPI *PFN_TERMINATEEXTENSION)(DWORD);

BOOL WINAPI GetExtensionVersion(HSE_VERSION_INFO *);
DWORD WINAPI HttpExtensionProc(EXTENSION_CONTROL_BLOCK *);
BOOL WINAPI TerminateExtension(DWORD);

#ifdef __cplusplus
}
#endif

#endif /* __WINE_HTTPEXT_H */
