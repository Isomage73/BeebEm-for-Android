// Android Windows.h implementations — equivalent of macos/Windows.mm + posix/Windows.cpp.
// Provides fake Win32 API functions used by the BeebEm core.

#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <android/log.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

#include "Windows.h"
#include "BeebEmPages.h"
#include "FileUtils.h"
#include "Sdl.h"

#define LOG_TAG "BeebEm"

int _vscprintf(const char *format, va_list pargs) {
    va_list argcopy;
    va_copy(argcopy, pargs);
    int retval = vsnprintf(nullptr, 0, format, argcopy);
    va_end(argcopy);
    return retval;
}

void SetWindowText(HWND /*hWnd*/, const char *pszTitle) {
    SetWindowTitle(pszTitle);
}

void Sleep(DWORD milliseconds) {
    SaferSleep((unsigned int)milliseconds);
}

DWORD GetTickCount() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (DWORD)((ts.tv_sec * 1000ULL) + (ts.tv_nsec / 1000000ULL));
}

// --------------------------------------------------------------------------
// Menu state helpers — delegate to BeebEmPages stubs.
// --------------------------------------------------------------------------

DWORD CheckMenuItem(HMENU /*hMenu*/, UINT uIDCheckItem, UINT uCheck) {
    DWORD prev = (DWORD)GetGUIOption((int)uIDCheckItem);
    UpdateGUIOption((int)uIDCheckItem, (uCheck == MF_CHECKED) ? 1 : 0);
    return prev ? MF_CHECKED : MF_UNCHECKED;
}

BOOL CheckMenuRadioItem(HMENU /*hMenu*/, UINT FirstID, UINT LastID,
                        UINT SelectedID, UINT /*Flags*/) {
    for (UINT id = FirstID; id <= LastID; ++id)
        UpdateGUIOption((int)id, (id == SelectedID) ? 1 : 0);
    return TRUE;
}

BOOL ModifyMenu(HMENU /*hMnu*/, UINT uPosition, UINT /*uFlags*/,
                PTR /*uIDNewItem*/, LPCTSTR lpNewItem) {
    if (lpNewItem) SetGUIOptionCaption((int)uPosition, lpNewItem);
    return TRUE;
}

UINT GetMenuState(HMENU /*hMenu*/, UINT uId, UINT /*uFlags*/) {
    return GetGUIOption((int)uId) ? MF_CHECKED : MF_UNCHECKED;
}

BOOL MoveFileEx(LPCTSTR lpExisting, LPCTSTR lpNew, DWORD /*flags*/) {
    return (rename(lpExisting, lpNew) == 0) ? TRUE : FALSE;
}

BOOL EnableMenuItem(HMENU /*hMenu*/, UINT /*uIDEnableItem*/, UINT /*uEnable*/) {
    return TRUE;
}

int WSAGetLastError() { return errno; }
int WSACleanup()      { return 0; }

void GetLocalTime(SYSTEMTIME *pTime) {
    time_t t;
    time(&t);
    struct tm *tm = localtime(&t);
    pTime->wYear        = (WORD)(tm->tm_year + 1900);
    pTime->wMonth       = (WORD)(tm->tm_mon + 1);
    pTime->wDayOfWeek   = (WORD)tm->tm_wday;
    pTime->wDay         = (WORD)tm->tm_mday;
    pTime->wHour        = (WORD)tm->tm_hour;
    pTime->wMinute      = (WORD)tm->tm_min;
    pTime->wSecond      = (WORD)tm->tm_sec;
    pTime->wMilliseconds = 0;
}

int ioctlsocket(SOCKET Socket, long Cmd, unsigned long *pArg) {
    return ioctl((int)Socket, Cmd, pArg);
}

void ZeroMemory(PVOID Destination, SIZE_T Length) {
    memset(Destination, 0, (size_t)Length);
}

UINT_PTR SetTimer(HWND /*hWnd*/, UINT_PTR /*nIDEvent*/,
                  UINT /*uElapse*/, TIMERPROC /*lpTimerFunc*/) { return 0; }
BOOL KillTimer(HWND /*hWnd*/, UINT_PTR /*nIDEvent*/) { return FALSE; }

BOOL PathIsRelative(LPCSTR pszPath) {
    return (pszPath && pszPath[0] != '/') ? TRUE : FALSE;
}

BOOL PathCanonicalize(LPSTR pszBuf, LPCSTR pszPath) {
    strcpy(pszBuf, pszPath);
    return TRUE;
}

int SHCreateDirectoryEx(HWND /*hWnd*/, LPCSTR pszPath, const void * /*psa*/) {
    int result = mkdir(pszPath, 0755);
    return (result == 0) ? 0 : errno;
}

DWORD GetFullPathName(LPCSTR /*pszFileName*/, DWORD /*BufferLength*/,
                      LPSTR /*pszBuffer*/, LPSTR * /*pszFilePart*/) { return 0; }

DWORD GetCurrentThreadId() {
    return (DWORD)(uintptr_t)pthread_self();
}

void InitializeCriticalSection(CRITICAL_SECTION * /*p*/) {}
void DeleteCriticalSection(CRITICAL_SECTION * /*p*/)     {}
void EnterCriticalSection(CRITICAL_SECTION * /*p*/)      {}
void LeaveCriticalSection(CRITICAL_SECTION * /*p*/)      {}

BOOL GetWindowRect(HWND /*hWnd*/, RECT *pRect) {
    ZeroMemory(pRect, sizeof(RECT));
    return TRUE;
}

BOOL MessageBeep(UINT /*uType*/) { return TRUE; }

void OutputDebugString(const char *pszMessage) {
    __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, "%s", pszMessage);
}

void _makepath(char *path, const char *drive, const char *dir,
               const char *fname, const char *ext) {
    strcpy(path, drive ? drive : "");
    AppendPath(path, dir ? dir : "");
    AppendPath(path, fname ? fname : "");
    if (ext) strcat(path, ext);
}

int _stricmp(const char *s1, const char *s2) { return strcasecmp(s1, s2); }

DWORD GetLastError() { return (DWORD)errno; }

HWND GetDlgItem(HWND /*hDlg*/, int /*nDlgItemID*/) { return nullptr; }
BOOL EnableWindow(HWND /*hWnd*/, BOOL /*bEnable*/) { return TRUE; }
HWND GetParent(HWND /*hWnd*/) { return nullptr; }
BOOL IsWindowEnabled(HWND /*hWnd*/) { return TRUE; }
BOOL SetWindowPos(HWND /*hWnd*/, HWND /*hWndInsertAfter*/,
                  int /*X*/, int /*Y*/, int /*cx*/, int /*cy*/,
                  UINT /*uFlags*/) { return TRUE; }
