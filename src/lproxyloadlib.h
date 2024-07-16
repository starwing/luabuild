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


PLL_API BOOL pll_FindLibraryFile (const char* filename, char* fullPath, DWORD fullPathSize);

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
    HMEMORYMODULE hMemoryModule;
    HMODULE       hModule;
} pll_CustomModule;

PLL_API BOOL pll_FindLibraryFile(const char* filename, char* fullPath, DWORD fullPathSize) {
    if (GetFullPathNameA(filename, fullPathSize, fullPath, NULL) > 0) {
        if (GetFileAttributesA(fullPath) != INVALID_FILE_ATTRIBUTES) {
            return TRUE;
        }
    }
    if (GetSystemDirectoryA(fullPath, fullPathSize) > 0) {
        strncat(fullPath, "\\", fullPathSize - strlen(fullPath) - 1);
        strncat(fullPath, filename, fullPathSize - strlen(fullPath) - 1);
        if (GetFileAttributesA(fullPath) != INVALID_FILE_ATTRIBUTES) {
            return TRUE;
        }
    }
    if (GetWindowsDirectoryA(fullPath, fullPathSize) > 0) {
        strncat(fullPath, "\\", fullPathSize - strlen(fullPath) - 1);
        strncat(fullPath, filename, fullPathSize - strlen(fullPath) - 1);
        if (GetFileAttributesA(fullPath) != INVALID_FILE_ATTRIBUTES) {
            return TRUE;
        }
    }
    char* pathEnv = getenv("PATH");
    if (pathEnv != NULL) {
        char* path = strtok(pathEnv, ";");
        while (path != NULL) {
            snprintf(fullPath, fullPathSize, "%s\\%s", path, filename);
            if (GetFileAttributesA(fullPath) != INVALID_FILE_ATTRIBUTES) {
                return TRUE;
            }
            path = strtok(NULL, ";");
        }
    }
    SetLastError(ERROR_FILE_NOT_FOUND);
    return FALSE;
}

static HCUSTOMMODULE pll_allocHandle(pll_CustomModule module) {
    pll_CustomModule *cm = (pll_CustomModule*)malloc(sizeof(pll_CustomModule));
    if (cm == NULL) return NULL;
    *cm = module;
    return (HCUSTOMMODULE)cm;
}

static HCUSTOMMODULE pll_LoadLibraryFunc(LPCSTR dllName, void *ud);
static FARPROC pll_GetProcAddressFunc(HCUSTOMMODULE hHandle, LPCSTR FuncName, void *ud);
static void pll_FreeLibraryFunc(HCUSTOMMODULE hHandle, void *ud);

static HCUSTOMMODULE pll_LoadLibraryFunc(LPCSTR dllName, void *ud) {
    HANDLE hFile = INVALID_HANDLE_VALUE;
    HANDLE hFileMapping = NULL;
    LPVOID lpBaseAddress = NULL;
    LARGE_INTEGER fileSize;
    pll_CustomModule cm = {NULL, NULL};
    char fullPath[MAX_PATH];
    (void)ud;
    if (strcmp(dllName, PLL_APIMOD) == 0) {
        cm.hModule = GetModuleHandle(NULL);
        return pll_allocHandle(cm);
    }
    if ((cm.hModule = GetModuleHandle(dllName)) != NULL)
        return pll_allocHandle(cm);
    if (!pll_FindLibraryFile(dllName, fullPath, sizeof(fullPath)))
        return NULL;
    hFile = CreateFileA(fullPath, GENERIC_READ, 0,
            NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        return NULL;
    if (!GetFileSizeEx(hFile, &fileSize)) {
        CloseHandle(hFile);
        return NULL;
    }
    hFileMapping = CreateFileMappingA(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    if (hFileMapping == NULL) {
        CloseHandle(hFile);
        return NULL;
    }
    lpBaseAddress = MapViewOfFile(hFileMapping, FILE_MAP_READ, 0, 0, 0);
    if (lpBaseAddress == NULL) {
        CloseHandle(hFileMapping);
        CloseHandle(hFile);
        return NULL;
    }
    cm.hMemoryModule = MemoryLoadLibraryEx(lpBaseAddress,
            (size_t)fileSize.QuadPart,
            MemoryDefaultAlloc,
            MemoryDefaultFree,
            pll_LoadLibraryFunc,
            pll_GetProcAddressFunc,
            pll_FreeLibraryFunc, NULL);
    if (!UnmapViewOfFile(lpBaseAddress)) {
        MemoryFreeLibrary(cm.hMemoryModule);
        cm.hMemoryModule = NULL;
    }
    CloseHandle(hFileMapping);
    CloseHandle(hFile);
    if (cm.hMemoryModule == NULL) {
        cm.hModule = LoadLibraryA(dllName);
        if (cm.hModule == NULL)
            return NULL;
    }
    return pll_allocHandle(cm);
}

static FARPROC pll_GetProcAddressFunc(HCUSTOMMODULE hHandle, LPCSTR FuncName, void *ud) {
    pll_CustomModule *cm = (pll_CustomModule*)hHandle;
    (void)ud;
    if (cm->hMemoryModule != NULL)
        return MemoryGetProcAddress(cm->hMemoryModule, FuncName);
    if (cm->hModule != NULL)
        return GetProcAddress(cm->hModule, FuncName);
    return NULL;
}

static void pll_FreeLibraryFunc(HCUSTOMMODULE hHandle, void *ud) {
    pll_CustomModule *cm = (pll_CustomModule*)hHandle;
    (void)ud;
    if (cm->hMemoryModule != NULL)
        MemoryFreeLibrary(cm->hMemoryModule);
    if (cm->hModule != NULL)
        FreeLibrary(cm->hModule);
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
