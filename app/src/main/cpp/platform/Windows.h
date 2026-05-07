// Android fake-Windows header — adapted from posix/Windows.h
// Removes uuid/uuid.h dependency; keeps all type stubs needed by BeebEm core.

#ifndef FAKE_MS_WINDOWS_H
#define FAKE_MS_WINDOWS_H

#include <stdarg.h>

#define MAX_PATH 1024

extern int __argc;
extern char **__argv;

#define stricmp strcasecmp

typedef long __int64;

typedef struct {
    unsigned short lowpart;
    unsigned short highpart;
} LARGE_INTEGER;

typedef unsigned char* PBYTE;
typedef float FLOAT;

struct GUID {
    unsigned int   Data1;
    unsigned short Data2;
    unsigned short Data3;
    unsigned char  Data4[8];
};

#define MF_BYCOMMAND 0x00
#define MOVEFILE_REPLACE_EXISTING 0x00000001
#define MOVEFILE_COPY_ALLOWED     0x00000002

typedef int    HKEY;
typedef char*  LPSTR;
typedef char*  LPTSTR;
#define HKEY_CURRENT_USER 0

typedef int         PTR;
typedef void*       PVOID;
typedef const void* LPCVOID;
typedef const char* LPCTSTR;
typedef const char* LPCSTR;

typedef char           CHAR;
typedef unsigned char  BYTE;
typedef unsigned short WORD;
typedef unsigned int   DWORD;
typedef unsigned short USHORT;
typedef unsigned int   UINT;
typedef unsigned int   UINT_PTR;
typedef int            INT;
typedef int            INT_PTR;
typedef long           LONG;

typedef unsigned int  COLORREF;
typedef unsigned long SIZE_T;

#define MF_CHECKED   1
#define MF_UNCHECKED 0
#define MF_ENABLED   0x0000
#define MF_GRAYED    1

#ifndef BOOL
#  define BOOL  int
#  define FALSE 0
#  define TRUE  1
#endif

typedef void* HANDLE;
typedef void* HACCEL;
typedef void* HMENU;
typedef void* HDC;
typedef void* HWND;
typedef void* HMODULE;
typedef void* HMONITOR;
typedef int   JOYCAPS;
typedef void* HGDIOBJ;
typedef void* HBITMAP;
typedef void  IDirectDraw;
typedef void  IDirectDraw2;
typedef void  IDirectDrawSurface;
typedef void  IDirectDrawSurface2;
typedef void  IDirectDrawClipper;
typedef void  IDirect3D9;
typedef void  IDirect3DDevice9;
typedef void  IDirect3DVertexBuffer9;
typedef void  IDirect3DTexture9;

typedef int HRESULT;
const HRESULT S_OK = 0;
constexpr bool SUCCEEDED(HRESULT hResult) { return hResult >= 0; }
constexpr bool FAILED(HRESULT hResult)    { return hResult < 0; }

typedef int D3DVECTOR;
typedef int D3DCOLOR;
typedef int D3DMATRIX;

typedef void* WNDPROC;
typedef int   LRESULT;

typedef void ISpVoice;
typedef void ISpObjectToken;

typedef unsigned int  ULONG_PTR;
typedef void* LPDIRECTSOUND;
typedef void* LPDIRECTSOUNDBUFFER;
typedef void* HINSTANCE;

typedef int SOCKET;
constexpr SOCKET INVALID_SOCKET = -1;
constexpr int    SOCKET_ERROR   = -1;
typedef struct sockaddr SOCKADDR;

int WSAGetLastError();
int WSACleanup();

typedef char  WCHAR;
typedef void  CLSID;
typedef long  WPARAM;
typedef long  LPARAM;
typedef void* HHOOK;

struct SYSTEMTIME {
    WORD wYear, wMonth, wDayOfWeek, wDay;
    WORD wHour, wMinute, wSecond, wMilliseconds;
};

void GetLocalTime(SYSTEMTIME* pTime);

typedef void* TIMERPROC;

void     SetWindowText(HWND hwnd, const char *pszTitle);
void     Sleep(DWORD Milliseconds);
DWORD    GetTickCount();
DWORD    CheckMenuItem(HMENU hmenu, UINT uIDCheckItem, UINT uCheck);
BOOL     ModifyMenu(HMENU hMnu, UINT uPosition, UINT uFlags, PTR uIDNewItem, LPCTSTR lpNewItem);
BOOL     MoveFileEx(LPCTSTR lpExistingFileName, LPCTSTR lpNewFileName, DWORD dwFlags);
BOOL     EnableMenuItem(HMENU hMenu, UINT uIDEnableItem, UINT uEnable);
UINT     GetMenuState(HMENU hMenu, UINT uId, UINT uFlags);
UINT_PTR SetTimer(HWND hWnd, UINT_PTR nIDEvent, UINT uElapse, TIMERPROC lpTimerFunc);
BOOL     KillTimer(HWND hWnd, UINT_PTR nIDEvent);
BOOL     PathIsRelative(LPCSTR pszPath);

void  _makepath(char *path, const char *drive, const char *dir,
                const char *fname, const char *ext);

constexpr int _MAX_DRIVE = 3;
constexpr int _MAX_DIR   = MAX_PATH;

BOOL  PathCanonicalize(LPSTR pszBuf, LPCSTR pszPath);
int   SHCreateDirectoryEx(HWND hWnd, LPCSTR pszPath, const void *psa);
DWORD GetFullPathName(LPCSTR pszFileName, DWORD BufferLength,
                      LPSTR pszBuffer, LPSTR *pszFilePart);
DWORD GetCurrentThreadId();

#define CALLBACK
#define STDAPICALLTYPE

int   _stricmp(const char *string1, const char *string2);
DWORD GetLastError();

typedef struct tagRECT { LONG left, top, right, bottom; } RECT;
typedef struct tagPOINT { LONG x, y; } POINT;
typedef struct tagMSG {
    HWND hwnd; UINT message; WPARAM wParam; LPARAM lParam;
    DWORD time; POINT pt; DWORD lPrivate;
} MSG;

#ifndef MAKEWORD
#define MAKEWORD(b1, b2) ((WORD)(((BYTE)(b1)) | ((WORD)((BYTE)(b2))) << 8))
#endif
#ifndef LOBYTE
#define LOBYTE(w) ((BYTE)(w))
#endif
#ifndef HIBYTE
#define HIBYTE(w) ((BYTE)(((WORD)(w) >> 8) & 0xFF))
#endif

#define _countof(x) (sizeof(x) / sizeof(x[0]))

void  ZeroMemory(PVOID Destination, SIZE_T Length);
int   _vscprintf(const char* format, va_list pargs);

constexpr int ERROR_SUCCESS = 0;

#define UNREFERENCED_PARAMETER(x) (void)x

constexpr int WM_APP = 0x8000;

BOOL CheckMenuRadioItem(HMENU hMenu, UINT FirstID, UINT LastID,
                        UINT SelectedID, UINT Flags);

#define __stdcall

constexpr unsigned char NOPARITY   = 0;
constexpr unsigned char ODDPARITY  = 1;
constexpr unsigned char EVENPARITY = 2;
constexpr DWORD MS_CTS_ON = 0x0010;

int ioctlsocket(SOCKET Socket, long Cmd, unsigned long* pArg);

typedef int CRITICAL_SECTION;

void InitializeCriticalSection(CRITICAL_SECTION*);
void DeleteCriticalSection(CRITICAL_SECTION*);
void EnterCriticalSection(CRITICAL_SECTION*);
void LeaveCriticalSection(CRITICAL_SECTION*);

BOOL GetWindowRect(HWND hWnd, RECT* pRect);

constexpr DWORD SPF_PURGEBEFORESPEAK = 1;
constexpr DWORD SPF_NLP_SPEAK_PUNC   = 2;

BOOL MessageBeep(UINT uType);
void OutputDebugString(const char* pszMessage);

HWND GetDlgItem(HWND hDlg, int nDlgItemID);
BOOL EnableWindow(HWND hWnd, BOOL bEnable);
HWND GetParent(HWND hWnd);
BOOL IsWindowEnabled(HWND hWnd);
BOOL SetWindowPos(HWND hWnd, HWND hWndInsertAfter, int X, int Y,
                  int cx, int cy, UINT uFlags);

#endif // FAKE_MS_WINDOWS_H
