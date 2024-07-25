#ifndef lproxyloadlib_h
#define lproxyloadlib_h

#ifndef PLL_NS_BEGIN
# ifdef __cplusplus
#   define PLL_NS_BEGIN extern "C" {
#   define PLL_NS_END   }
# else
#   define PLL_NS_BEGIN
#   define PLL_NS_END
# endif
#endif /* PLL_NS_BEGIN */

#ifndef PLL_STATIC
# if __GNUC__
#   define PLL_STATIC static __attribute((unused))
# else
#   define PLL_STATIC static
# endif
#endif

#ifdef PLL_STATIC_API
# ifndef PLL_IMPLEMENTATION
#  define PLL_IMPLEMENTATION
# endif
# define PLL_API PLL_STATIC
#endif

#if !defined(PLL_API) && defined(_WIN32)
# ifdef PLL_IMPLEMENTATION
#  define PLL_API __declspec(dllexport)
# else
#  define PLL_API __declspec(dllimport)
# endif
#endif

#ifndef PLL_API
# define PLL_API extern
#endif

#ifndef _WIN32
# error "only works on Windows"
#endif

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "MemoryModule.h"

PLL_NS_BEGIN

typedef enum pll_LibLoc { pll_None, pll_Sys, pll_User } pll_LibLoc;

PLL_API pll_LibLoc pll_FindLibraryFile (const char* filename, char* fullPath, DWORD fullPathSize);

PLL_API HCUSTOMMODULE pll_LoadLibrary    (LPCSTR dllName);
PLL_API FARPROC       pll_GetProcAddress (HCUSTOMMODULE hHandle, LPCSTR FuncName);
PLL_API void          pll_FreeLibrary    (HCUSTOMMODULE hHandle);


PLL_NS_END

#endif /* lproxyloadlib_h */

#if defined(PLL_IMPLEMENTATION) && !defined(pll_implemented)
#define pll_implemented

#include "MemoryModule.c"

#include <lua.h>
#include <string.h>

#define PLL_APIMOD "lua" LUA_VERSION_MAJOR LUA_VERSION_MINOR ".dll"

PLL_NS_BEGIN


typedef void (*pll_voidf)(void);

typedef struct pll_CustomModule {
    LPCSTR        dllName;
    HMEMORYMODULE hMemoryModule;
    HMODULE       hModule;
    LPVOID        pNext, pPrev;
    LONG          lRefCount;
} pll_CustomModule;

static pll_CustomModule *pll_Modules = NULL;

PLL_API pll_LibLoc pll_FindLibraryFile(const char* filename, char* fullPath, DWORD fullPathSize) {
    if (GetFullPathNameA(filename, fullPathSize, fullPath, NULL) > 0) {
        if (GetFileAttributesA(fullPath) != INVALID_FILE_ATTRIBUTES) {
            return pll_User;
        }
    }
    if (GetSystemDirectoryA(fullPath, fullPathSize) > 0) {
        strncat(fullPath, "\\", fullPathSize - strlen(fullPath) - 1);
        strncat(fullPath, filename, fullPathSize - strlen(fullPath) - 1);
        if (GetFileAttributesA(fullPath) != INVALID_FILE_ATTRIBUTES) {
            return pll_Sys;
        }
    }
    if (GetWindowsDirectoryA(fullPath, fullPathSize) > 0) {
        strncat(fullPath, "\\", fullPathSize - strlen(fullPath) - 1);
        strncat(fullPath, filename, fullPathSize - strlen(fullPath) - 1);
        if (GetFileAttributesA(fullPath) != INVALID_FILE_ATTRIBUTES) {
            return pll_Sys;
        }
    }
    char* pathEnv = getenv("PATH");
    if (pathEnv != NULL) {
        char* path = strtok(pathEnv, ";");
        while (path != NULL) {
            snprintf(fullPath, fullPathSize, "%s\\%s", path, filename);
            if (GetFileAttributesA(fullPath) != INVALID_FILE_ATTRIBUTES) {
                return pll_User;
            }
            path = strtok(NULL, ";");
        }
    }
    SetLastError(ERROR_FILE_NOT_FOUND);
    return pll_None;
}

static HCUSTOMMODULE pll_AllocSysHandle(HMODULE hModule) {
    pll_CustomModule *cm;
    if (hModule == NULL) return NULL;
    cm = (pll_CustomModule*)malloc(sizeof(pll_CustomModule));
    memset(cm, 0, sizeof(pll_CustomModule));
    cm->hModule = hModule;
    return cm;
}

static HCUSTOMMODULE pll_AllocMemHandle(LPCSTR dllName, HMEMORYMODULE hMemoryModule) {
    pll_CustomModule *cm = (pll_CustomModule*)malloc(sizeof(pll_CustomModule));
    size_t dllNameLen = strlen(dllName);
    char *memName;
    if (cm == NULL) return NULL;
    memName = (char*)malloc(dllNameLen + 1);
    if (memName == NULL) return (free(cm), NULL);
    memset(cm, 0, sizeof(pll_CustomModule));
    memcpy(memName, dllName, dllNameLen+1);
    cm->dllName = memName;
    cm->hMemoryModule = hMemoryModule;
    cm->pNext = pll_Modules;
    if (pll_Modules) pll_Modules->pPrev = cm;
    cm->lRefCount = 1;
    pll_Modules = cm;
    return (HCUSTOMMODULE)cm;
}

static HCUSTOMMODULE pll_GetModuleHandle(LPCSTR dllName) {
    pll_CustomModule *cm = pll_Modules;
    for (; cm != NULL; cm = (pll_CustomModule*)cm->pNext) {
        if (strcmp(dllName, cm->dllName) == 0) {
            ++cm->lRefCount;
            return (HCUSTOMMODULE)cm;
        }
    }
    return NULL;
}

static HCUSTOMMODULE pll_LoadLibraryFunc(LPCSTR dllName, void *ud);
static FARPROC pll_GetProcAddressFunc(HCUSTOMMODULE hHandle, LPCSTR FuncName, void *ud);
static void pll_FreeLibraryFunc(HCUSTOMMODULE hHandle, void *ud);

static HCUSTOMMODULE pll_LoadLibraryFunc(LPCSTR dllName, void *ud) {
    HANDLE hFile = INVALID_HANDLE_VALUE;
    HANDLE hFileMapping = NULL;
    LPVOID lpBaseAddress = NULL;
    LARGE_INTEGER fileSize;
    HMODULE hModule;
    HMEMORYMODULE hMemoryModule;
    HCUSTOMMODULE hCustomModule;
    char fullPath[MAX_PATH];
    (void)ud;
    if (strcmp(dllName, PLL_APIMOD) == 0)
        return pll_AllocSysHandle(GetModuleHandle(NULL));
    if ((hModule = GetModuleHandle(dllName)) != NULL)
        return pll_AllocSysHandle(hModule);
    if ((hCustomModule = pll_GetModuleHandle(dllName)) != NULL)
        return hCustomModule;
    if (pll_FindLibraryFile(dllName, fullPath, sizeof(fullPath)) != pll_User)
        return pll_AllocSysHandle(LoadLibraryA(dllName));
    hFile = CreateFileA(fullPath, GENERIC_READ, 0,
            NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        return pll_AllocSysHandle(LoadLibraryA(dllName));
    if (!GetFileSizeEx(hFile, &fileSize)) {
        CloseHandle(hFile);
        return pll_AllocSysHandle(LoadLibraryA(dllName));
    }
    hFileMapping = CreateFileMappingA(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    if (hFileMapping == NULL) {
        CloseHandle(hFile);
        return pll_AllocSysHandle(LoadLibraryA(dllName));
    }
    lpBaseAddress = MapViewOfFile(hFileMapping, FILE_MAP_READ, 0, 0, 0);
    if (lpBaseAddress == NULL) {
        CloseHandle(hFileMapping);
        CloseHandle(hFile);
        return pll_AllocSysHandle(LoadLibraryA(dllName));
    }
    hMemoryModule = MemoryLoadLibraryEx(lpBaseAddress,
            (size_t)fileSize.QuadPart,
            MemoryDefaultAlloc,
            MemoryDefaultFree,
            pll_LoadLibraryFunc,
            pll_GetProcAddressFunc,
            pll_FreeLibraryFunc, NULL);
    DWORD err = GetLastError();
    if (!UnmapViewOfFile(lpBaseAddress)) {
        MemoryFreeLibrary(hMemoryModule);
        hMemoryModule = NULL;
    }
    CloseHandle(hFileMapping);
    CloseHandle(hFile);
    return hMemoryModule ?
        pll_AllocMemHandle(dllName, hMemoryModule) :
        pll_AllocSysHandle(LoadLibraryA(dllName));
}

static FARPROC pll_GetProcAddressFunc(HCUSTOMMODULE hHandle, LPCSTR funcName, void *ud) {
    pll_CustomModule *cm = (pll_CustomModule*)hHandle;
    (void)ud;
    if (cm->hMemoryModule != NULL)
        return MemoryGetProcAddress(cm->hMemoryModule, funcName);
    if (cm->hModule != NULL)
        return GetProcAddress(cm->hModule, funcName);
    return NULL;
}

static void pll_FreeLibraryFunc(HCUSTOMMODULE hHandle, void *ud) {
    pll_CustomModule *cm = (pll_CustomModule*)hHandle;
    (void)ud;
    if (cm->lRefCount > 1) {
        --cm->lRefCount;
        return;
    }
    if (cm->hModule != NULL)
        FreeLibrary(cm->hModule);
    else {
        MemoryFreeLibrary(cm->hMemoryModule);
        free((void*)cm->dllName);
        if (cm->pPrev == NULL)
            pll_Modules = NULL;
        else {
            pll_CustomModule *prev = (pll_CustomModule*)cm->pPrev;
            pll_CustomModule *next = (pll_CustomModule*)cm->pNext;
            prev->pNext = next;
            if (next) next->pPrev = prev;
        }
    }
    free(cm);
}

PLL_API HCUSTOMMODULE pll_LoadLibrary(LPCSTR dllName)
{ return pll_LoadLibraryFunc(dllName, NULL); }

PLL_API FARPROC pll_GetProcAddress(HCUSTOMMODULE hHandle, LPCSTR FuncName)
{ return pll_GetProcAddressFunc(hHandle, FuncName, NULL); }

PLL_API void pll_FreeLibrary(HCUSTOMMODULE hHandle)
{ pll_FreeLibraryFunc(hHandle, NULL); }


PLL_NS_END

#endif
