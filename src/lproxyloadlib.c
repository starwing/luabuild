#define LUA_LIB
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#if LUA_VERSION_NUM >= 503
# define lua53_getfield lua_getfield
#elif !defined(lua53_getfield)
# define lua53_getfield lua53_getfield
static int lua53_getfield(lua_State *L, int idx, const char *fname)
{ lua_getfield(L, idx, fname); return lua_type(L, -1); }
#endif

#if LUA_VERSION_NUM < 504 && !defined(luaL_addgsub)
# define luaL_addgsub luaL_addgsub
static void luaL_addgsub (luaL_Buffer *b, const char *s,
                                     const char *p, const char *r) {
  const char *wild;
  size_t l = strlen(p);
  while ((wild = strstr(s, p)) != NULL) {
    luaL_addlstring(b, s, wild - s);  /* push prefix */
    luaL_addstring(b, r);  /* push replacement in place of pattern */
    s = wild + l;  /* continue after 'p' */
  }
  luaL_addstring(b, s);  /* push last suffix */
}
#endif

#if LUA_VERSION_NUM == 501 && !defined(luaL_getsubtable)
#define luaL_getsubtable luaL_getsubtable
static int luaL_getsubtable (lua_State *L, int idx, const char *fname) {
    if (lua53_getfield(L, idx, fname) == LUA_TTABLE) return 1;
    lua_pop(L, 1);  /* remove previous result */
    if (idx < 0 && idx > LUA_REGISTRYINDEX) 
        idx += lua_gettop(L) + 1;
    lua_newtable(L);
    lua_pushvalue(L, -1);
    lua_setfield(L, idx, fname);
    return 0;
}
#endif

#if LUA_VERSION_NUM >= 502
# define lua52_pushstring  lua_pushstring
# define lua52_pushlstring lua_pushlstring
# define lua52_pushfstring lua_pushfstring
#else
# ifndef lua52_pushstring
#   define lua52_pushstring lua52_pushstring
static const char *lua52_pushstring(lua_State *L, const char *s)
{ lua_pushstring(L, s); return lua_tostring(L, -1); }
# endif
# ifndef lua52_pushlstring
#   define lua52_pushlstring lua52_pushlstring
static const char *lua52_pushlstring(lua_State *L, const char *s, size_t len)
{ lua_pushlstring(L, s, len); return lua_tostring(L, -1); }
# endif
# ifndef lua52_pushfstring
#   define lua52_pushfstring lua52_pushfstring
static const char *lua52_pushfstring(lua_State *L, const char *fmt, ...) {
    va_list l;
    va_start(l, fmt);
    lua_pushvfstring(L, fmt, l);
    va_end(l);
    return lua_tostring(L, -1);
}
# endif
#endif

#ifndef luaL_buffaddr
# if LUA_VERSION_NUM == 501
#   define luaL_buffaddr(B) ((B)->buffer)
# else
#   define luaL_buffaddr(B) ((B)->b)
# endif
#endif

#ifndef luaL_bufflen
# if LUA_VERSION_NUM == 501
#   define luaL_bufflen(B) ((B)->p - (B)->buffer)
# else
#   define luaL_bufflen(B) ((B)->n)
# endif
#endif

#ifndef LUA_VERSION_MAJOR
# define LUA_VERSION_MAJOR "5"
#endif

#ifndef LUA_VERSION_MINOR
# define LUA_VERSION_MINOR "1"
#endif

#ifndef _WIN32
# error "only works on Windows"
#endif

#define PLL_APIMOD "lua" LUA_VERSION_MAJOR LUA_VERSION_MINOR ".dll"

#include "MemoryModule.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <string.h>

typedef void (*pll_voidf)(void);

typedef struct pll_CustomModule {
    HMEMORYMODULE hMemoryModule;
    HMODULE       hModule;
} pll_CustomModule;

static HCUSTOMMODULE pll_LoadLibraryFunc(LPCSTR dllName, void *ud);
static FARPROC pll_GetProcAddressFunc(HCUSTOMMODULE hHandle, LPCSTR FuncName, void *ud);
static void pll_FreeLibraryFunc(HCUSTOMMODULE hHandle, void *ud);

static BOOL pll_FindLibraryFile(const char* filename, char* fullPath, DWORD fullPathSize) {
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

static void pll_pusherror(lua_State *L) {
    int error = GetLastError();
    char buffer[128];
    if (FormatMessageA(
                FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM,
                NULL, error, 0, buffer, sizeof(buffer)/sizeof(char), NULL))
        lua_pushstring(L, buffer);
    else
        lua_pushfstring(L, "system error %d\n", error);
}

static void pll_unloadlib(void *lib) {
    pll_FreeLibraryFunc((HMODULE)lib, NULL);
}

static void *pll_load(lua_State *L, const char *path) {
    HMODULE lib = pll_LoadLibraryFunc(path, L);
    if (lib == NULL) pll_pusherror(L);
    return lib;
}

static lua_CFunction pll_sym(lua_State *L, void *lib, const char *sym) {
    lua_CFunction f = (lua_CFunction)(pll_voidf)
        pll_GetProcAddressFunc((HMODULE)lib, sym, L);
    if (f == NULL) pll_pusherror(L);
    return f;
}

#define PLL_POF    "luaopen_"
#define PLL_OFSEP  "_"
#define PLL_IGMARK "-"

static const char *const PLL_CLIBS = "PLL_CLIBS";

/* error codes for 'pll_lookforfunc' */
typedef enum pll_LoadErr { PL_OK, PL_ERRLIB, PL_ERRFUNC } pll_LoadErr;

static void *pll_checkclib(lua_State *L, const char *path) {
    void *plib;
    lua_getfield(L, LUA_REGISTRYINDEX, PLL_CLIBS);
    lua_getfield(L, -1, path);
    plib = lua_touserdata(L, -1);
    lua_pop(L, 2);
    return plib;
}

static void pll_addtoclib(lua_State *L, const char *path, void *plib) {
    lua_getfield(L, LUA_REGISTRYINDEX, PLL_CLIBS);
    lua_pushlightuserdata(L, plib);
    lua_pushvalue(L, -1);
    lua_setfield(L, -3, path);
    lua_rawseti(L, -2, (int)luaL_len(L, -2) + 1);
    lua_pop(L, 1);
}

static pll_LoadErr pll_lookforfunc(lua_State *L, const char *path, const char *sym) {
    void *reg = pll_checkclib(L, path);
    if (reg == NULL) {
        reg = pll_load(L, path);
        if (reg == NULL) return PL_ERRLIB;
        pll_addtoclib(L, path, reg);
    }
    if (*sym == '*')
        lua_pushboolean(L, 1);
    else {
        lua_CFunction f = pll_sym(L, reg, sym);
        if (f == NULL) return PL_ERRFUNC;
        lua_pushcfunction(L, f);
    }
    return PL_OK;
}

static pll_LoadErr pll_loadfunc(lua_State *L, const char *filename, const char *modname) {
    const char *openfunc;
    const char *mark;
    modname = luaL_gsub(L, modname, ".", PLL_OFSEP);
    mark = strchr(modname, *PLL_IGMARK);
    if (mark) {
        pll_LoadErr stat;
        openfunc = lua52_pushlstring(L, modname, mark - modname);
        openfunc = lua52_pushfstring(L, PLL_POF"%s", openfunc);
        stat = pll_lookforfunc(L, filename, openfunc);
        if (stat != PL_ERRFUNC) return stat;
        modname = mark + 1;  /* else go ahead and try old-style name */
    }
    openfunc = lua_pushfstring(L, PLL_POF"%s", modname);
    return pll_lookforfunc(L, filename, openfunc);
}

static void pll_notfound(lua_State *L, const char *path) {
    luaL_Buffer b;
    luaL_buffinit(L, &b);
    luaL_addstring(&b, "no file '");
    luaL_addgsub(&b, path, LUA_PATH_SEP, "'\n\tno file '");
    luaL_addstring(&b, "'");
    luaL_pushresult(&b);
}

static const char *pll_nextfn(char **path, char *end) {
    char *sep, *name = *path;
    if (name == end) return NULL;
    if (*name == '\0') {
        *name = *LUA_PATH_SEP;
        name++;
    }
    sep = strchr(name, *LUA_PATH_SEP);
    if (sep == NULL) sep = end;
    *sep = '\0';
    *path = sep;
    return name;
}

static int pll_readable(const char *filename) {
    FILE *f = fopen(filename, "r");
    int r = (f != NULL);
    fclose(f);
    return r;
}

static const char *pll_searchpath(lua_State *L, const char *name, const char *path) {
    luaL_Buffer buff;
    char *pathname;
    char *endpathname;
    const char *filename;
    if (strchr(name, '.') != NULL)
        name = luaL_gsub(L, name, ".", LUA_DIRSEP);
    luaL_buffinit(L, &buff);
    luaL_addgsub(&buff, path, LUA_PATH_MARK, name);
    luaL_addchar(&buff, '\0');
    pathname = luaL_buffaddr(&buff);
    endpathname = pathname + luaL_bufflen(&buff) - 1;
    while ((filename = pll_nextfn(&pathname, endpathname)) != NULL)
        if (pll_readable(filename)) return lua52_pushstring(L, filename);
    luaL_pushresult(&buff);
    pll_notfound(L, lua_tostring(L, -1));
    return NULL;
}

static const char *pll_findfile(lua_State *L, const char *name, const char *pname) {
    const char *path;
    lua_getfield(L, lua_upvalueindex(1), pname);
    path = lua_tostring(L, -1);
    if (path == NULL) luaL_error(L, "'package.%s' must be a string", pname);
    return pll_searchpath(L, name, path);
}

static int pll_checkload(lua_State *L, pll_LoadErr stat, const char *filename) {
    if (stat)
        return (lua_pushstring(L, filename), 2);
    return luaL_error(L, "error loading module '%s' from file '%s':\n\t%s",
            lua_tostring(L, 1), filename, lua_tostring(L, -1));
}

static int pll_searcher_C(lua_State *L) {
    const char *name = luaL_checkstring(L, 1);
    const char *filename = pll_findfile(L, name, "cpath");
    if (filename == NULL) return 1;
    return pll_checkload(L,
            (pll_loadfunc(L, filename, name) == 0), filename);
}

static int pll_searcher_Croot(lua_State *L) {
    const char *filename;
    const char *name = luaL_checkstring(L, 1);
    const char *p = strchr(name, '.');
    int stat;
    if (p == NULL) return 0;  /* is root */
    lua_pushlstring(L, name, p - name);
    filename = pll_findfile(L, lua_tostring(L, -1), "cpath");
    if (filename == NULL) return 1;  /* root not found */
    if ((stat = pll_loadfunc(L, filename, name)) != 0) {
        if (stat != PL_ERRFUNC)
            return pll_checkload(L, 0, filename);  /* real error */
        /* open function not found */
        lua_pushfstring(L, "no module '%s' in file '%s'", name, filename);
        return 1;
    }
    return (lua_pushstring(L, filename), 2);
}

static int pll_gctm(lua_State *L) {
    lua_Integer n = luaL_len(L, 1);
    for (; n >= 1; n--) {
        lua_rawgeti(L, 1, n);
        pll_unloadlib(lua_touserdata(L, -1));
        lua_pop(L, 1);
    }
    return 0;
}

static void pll_createclibtable(lua_State *L) {
  luaL_getsubtable(L, LUA_REGISTRYINDEX, PLL_CLIBS);
  lua_createtable(L, 0, 1);
  lua_pushcfunction(L, pll_gctm);
  lua_setfield(L, -2, "__gc");
  lua_setmetatable(L, -2);
}

LUALIB_API int luaopen_proxyloader(lua_State *L) {
    lua_getglobal(L, "package"); /* 1 */
#if LUA_VERSION_NUM < 502
    if (lua53_getfield(L, -1, "loaders") == LUA_TNIL) /* 2 */
        return luaL_error(L, "cannot find loaders");
#else
    if (lua53_getfield(L, -1, "searchers") == LUA_TNIL) /* 2 */
        return luaL_error(L, "cannot find searchers");
#endif
    lua_getglobal(L, "table");
    if (lua53_getfield(L, -1, "insert") == LUA_TNIL) /* 3 */
        return luaL_error(L, "cannot find table.insert");
    lua_remove(L, -2);
    lua_pushvalue(L, -1); /* insert */
    lua_pushvalue(L, -3); /* searchers */
    lua_pushinteger(L, 3);
    lua_pushvalue(L, -6); /* package */
    lua_pushcclosure(L, pll_searcher_C, 1);
    lua_call(L, 3, 0);
    lua_pushvalue(L, -1); /* insert */
    lua_pushvalue(L, -3); /* searchers */
    lua_pushinteger(L, 4);
    lua_pushvalue(L, -6); /* package */
    lua_pushcclosure(L, pll_searcher_Croot, 1);
    lua_call(L, 3, 0);
    pll_createclibtable(L);
    return 0;
}

/* win32cc: flags+='-s -O2 -mdll -DLUA_BUILD_AS_DLL' libs+="-llua54"
 * win32cc: input+='MemoryModule.c' output='proxyloader.dll'
 */

