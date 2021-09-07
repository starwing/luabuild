#include <lua.h>
#include <stdio.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <delayimp.h>

FARPROC WINAPI delayHook(unsigned dliNotify, PDelayLoadInfo pdli);

#if _MSC_FULL_VER >= 190024210 // MSVC 2015 Update 3
# ifndef DELAYIMP_INSECURE_WRITABLE_HOOKS
# define HOOK_TYPE const PfnDliHook
# endif
#endif

#ifndef HOOK_TYPE
# define HOOK_TYPE PfnDliHook
#endif

HOOK_TYPE __pfnDliNotifyHook2  = delayHook;
HOOK_TYPE __pfnDliFailureHook2 = delayHook;

static int lowercase(int ch)
{ return 'A' <= ch && ch <= 'Z' ? (ch - 'A') + 'a' : ch; }

static int strcmp_nocase(const char *s1, const char *s2) {
    int r;
    while (!(r = lowercase(*s1) - *s2) && *s1)
        ++s1, ++s2;
    return r;
}

#ifndef LUA_VERSION_MAJOR
# define LUA_VERSION_MAJOR "5"
# define LUA_VERSION_MINOR "1"
#endif

FARPROC WINAPI delayHook(unsigned dliNotify, PDelayLoadInfo pdli) {
    if ((dliNotify == dliNotePreLoadLibrary || dliNotify == dliFailLoadLib)
            && strcmp_nocase(pdli->szDll,
                "lua" LUA_VERSION_MAJOR LUA_VERSION_MINOR ".exe") == 0)
        return (FARPROC)GetModuleHandleA(NULL);
    return 0;
}

