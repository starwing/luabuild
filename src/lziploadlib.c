#define LUA_LIB
#include <lua.h>
#include <lauxlib.h>

#if LUA_VERSION_NUM < 504 && !defined(luaL_pushfail)
# define luaL_pushfail(L) lua_pushnil(L)
#endif

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


#ifndef LUA_PATH_SEP
# ifdef _WIN32
#   define LUA_PATH_SEP ";"
# else
#   define LUA_PATH_SEP ":"
# endif
#endif

#ifndef LUA_PATH_MARK
# define LUA_PATH_MARK  "?"
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

#ifndef luaL_buffsub
# if LUA_VERSION_NUM == 501
#   define luaL_buffsub(B,d) ((B)->p -= (d))
# else
#   define luaL_buffsub(B,d) ((B)->n -= (d))
# endif
#endif

#ifndef LUA_VERSION_MAJOR
# define LUA_VERSION_MAJOR "5"
#endif

#ifndef LUA_VERSION_MINOR
# define LUA_VERSION_MINOR "1"
#endif

#include "miniz.h"

#define ZLL_APIMOD "lua" LUA_VERSION_MAJOR LUA_VERSION_MINOR ".dll"
#define ZLL_TYPE   "zipseacher.State"
#define ZLL_ENTRY  "main.lua"
#define ZLL_PATH   "?.lua;?.luac"
#ifdef _WIN32
# define ZLL_CPATH "?.dll;loadall.dll"
#else
# define ZLL_CPATH "?.so;loadall.so"
#endif

typedef struct zll_State {
    mz_zip_archive ar;
    const char *filename;
    int fn_ref;
    int clibs_ref;
} zll_State;

/* utils */

static const char *zll_lasterror(zll_State *S)
{ return mz_zip_get_error_string(mz_zip_get_last_error(&S->ar)); }

static int zll_install(lua_State *L) {
    lua_getglobal(L, "package");
#if LUA_VERSION_NUM < 502
    if (lua53_getfield(L, -1, "loaders") == LUA_TNIL)
        return luaL_error(L, "cannot find loaders");
#else
    if (lua53_getfield(L, -1, "searchers") == LUA_TNIL)
        return luaL_error(L, "cannot find searchers");
#endif
    lua_pushvalue(L, -3);
    lua_rawseti(L, -2, (int)luaL_len(L, -2) + 1);
    lua_pop(L, 2);
    return 0;
}

static mz_uint zll_fileindex(zll_State *S, lua_State *L, int idx) {
    mz_uint index = 0;
    int isint;
    lua_Integer i = lua_tointegerx(L, idx, &isint);
    if (isint) return (mz_uint)i;
    if (!mz_zip_reader_locate_file_v2(&S->ar,
                luaL_checkstring(L, idx), NULL, 0, &index))
        return luaL_error(L, "%s", zll_lasterror(S));
    return index;
}

static size_t zll_file_write_func(void *pOpaque, mz_uint64 file_ofs, const void *pBuf, size_t n) {
    luaL_Buffer *B = (luaL_Buffer*)pOpaque;
    (void)file_ofs;
    luaL_addlstring(B, pBuf, n);
    return n;
}

static const void *zll_filecontent(zll_State *S, lua_State *L, mz_uint index, size_t *plen) {
    luaL_Buffer B;
    luaL_buffinit(L, &B);
    if (!mz_zip_reader_extract_to_callback(&S->ar, index,
                zll_file_write_func, &B, 0)) {
        lua_pushstring(L, zll_lasterror(S));
        return NULL;
    }
    luaL_pushresult(&B);
    return (const void*)lua_tolstring(L, -1, plen);
}

static void *zll_checkclib(zll_State *S, lua_State *L, const char *path) {
    void *plib;
    lua_rawgeti(L, LUA_REGISTRYINDEX, S->clibs_ref);
    lua_getfield(L, -1, path);
    plib = lua_touserdata(L, -1);  /* plib = CLIBS[path] */
    lua_pop(L, 2);  /* pop CLIBS table and 'plib' */
    return plib;
}

static void zll_addtoclib(zll_State *S, lua_State *L, const char *path, void *plib) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, S->clibs_ref);
    lua_pushlightuserdata(L, plib);
    lua_pushvalue(L, -1);
    lua_setfield(L, -3, path);
    lua_rawseti(L, -2, (int)luaL_len(L, -2) + 1);
    lua_pop(L, 1);  /* pop CLIBS table */
}

/* load dll routines */

#define ZLL_ERRLIB  1
#define ZLL_ERRFUNC 2

#ifdef _WIN32

#ifndef lproxyloadlib_h
# define PLL_API static
# define PLL_IMPLEMENTATION
# include "lproxyloadlib.h"
#endif

static HCUSTOMMODULE zll_LoadLibraryFunc(LPCSTR dllName, void *ud);

static const char *zll_binpath() {
    static CHAR filename[MAX_PATH];
    if (!filename[0])
        GetModuleFileNameA(NULL, filename, MAX_PATH);
    return filename;
}

static HCUSTOMMODULE zll_LoadLibraryFunc(LPCSTR dllName, void *ud) {
    zll_State *S = (zll_State*)ud;
    size_t len;
    void *s;
    pll_CustomModule *cm = (pll_CustomModule*)malloc(sizeof(pll_CustomModule));
    if (cm == NULL) return NULL;
    memset(cm, 0, sizeof(*cm));
    if (strcmp(dllName, ZLL_APIMOD) == 0) {
        cm->hModule = GetModuleHandle(NULL);
        return (HCUSTOMMODULE)cm;
    }
    if ((cm->hModule = GetModuleHandle(dllName)) != NULL)
        return (HCUSTOMMODULE)cm;
    s = mz_zip_reader_extract_file_to_heap(&S->ar, dllName, &len, 0);
    if (s != NULL) {
        cm->hMemoryModule = MemoryLoadLibraryEx(s, len,
                MemoryDefaultAlloc,
                MemoryDefaultFree,
                zll_LoadLibraryFunc,
                pll_GetProcAddressFunc,
                pll_FreeLibraryFunc, S);
        mz_free(s);
        if (cm->hMemoryModule != NULL)
            return (HCUSTOMMODULE)cm;
    }
    free(cm);
    return (HCUSTOMMODULE)pll_LoadLibrary(dllName);
}

static void zll_pusherror(lua_State *L) {
    int error = GetLastError();
    char buffer[128];
    if (FormatMessageA(FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM,
                NULL, error, 0, buffer, sizeof(buffer)/sizeof(char), NULL))
        lua_pushstring(L, buffer);
    else
        lua_pushfstring(L, "system error %d\n", error);
}

static void *zll_load(zll_State *S, lua_State *L, const char *name, mz_uint index, int seeglb) {
    size_t len;
    const void *data = zll_filecontent(S, L, index, &len);
    HMEMORYMODULE lib = MemoryLoadLibraryEx(data, len,
            MemoryDefaultAlloc,
            MemoryDefaultFree,
            zll_LoadLibraryFunc,
            pll_GetProcAddressFunc,
            pll_FreeLibraryFunc,
            S);
    (void)seeglb, (void)name;
    if (lib == NULL) zll_pusherror(L);
    return lib;
}

static lua_CFunction zll_sym(lua_State *L, void *lib, const char *sym) {
  lua_CFunction f = (lua_CFunction)(pll_voidf)
      pll_GetProcAddress((HMEMORYMODULE)lib, sym);
  if (f == NULL) zll_pusherror(L);
  return f;
}

static void zll_unloadlib(void *lib)
{ pll_FreeLibrary((HMEMORYMODULE)lib); }

#else

#include <dlfcn.h>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#ifdef __APPLE__
# include <libproc.h>
#endif

#if defined(__GNUC__)
#define cast_func(p) (__extension__ (lua_CFunction)(p))
#else
#define cast_func(p) ((lua_CFunction)(p))
#endif

static const char *zll_binpath() {
#ifdef __APPLE__
    static char filename[PROC_PIDPATHINFO_MAXSIZE];
    if (filename[0] == '\0')
        proc_pidpath(getpid(), filename, PROC_PIDPATHINFO_MAXSIZE);
    return filename;
#else
    static char filename[PATH_MAX];
    char *ret = luaL_prepbuffsize(&B, PATH_MAX);
    if (filename[0] == '\0')
        readlink("/proc/self/exe", filename, PATH_MAX);
    return filename;
#endif
}

static void zll_unloadlib(void *lib) {
    dlclose(lib);
}

static int zll_makedirs(char *fname, size_t prefix) {
    char *p;
    while ((p = strchr(fname + prefix, '/')) != NULL) {
        *p = '\0';
        if (mkdir(fname, 0750) < 0 && errno != EEXIST)
            return 0;
        *p = '/';
        prefix = p - fname + 1;
    }
    return 1;
}

static int zll_writecontent(const char *fname, const void *data, size_t len) {
    int fd = open(fname, O_EXCL|O_CREAT|O_WRONLY, S_IRUSR|S_IWUSR);
    int ret = 1;
    if (fd < 0) return ret;
    ret = write(fd, data, len) == (ssize_t)len;
    close(fd);
    return ret;
}

static void *zll_load(zll_State *S, lua_State *L, const char *name, mz_uint index, int seeglb) {
    static char tmpdir[] = "ziploader.XXXXXX";
    static size_t tmplen;
    size_t len;
    const void *data = zll_filecontent(S, L, index, &len);
    char fname[PATH_MAX];
    if (tmplen == 0) {
        if (!mkdtemp(tmpdir)) {
            lua_pushstring(L, strerror(errno));
            return NULL;
        }
        tmplen = strlen(tmpdir);
    }
    snprintf(fname, PATH_MAX, "%s/%s/%s", tmpdir, S->filename, name);
    if (!zll_makedirs(fname, tmplen + 1)
            || !zll_writecontent(fname, data, len)) {
        lua_pushstring(L, strerror(errno));
        return NULL;
    }
    void *lib = dlopen(fname, RTLD_NOW | (seeglb ? RTLD_GLOBAL : RTLD_LOCAL));
    if (lib == NULL) lua_pushstring(L, dlerror());
    return lib;
}

static lua_CFunction zll_sym (lua_State *L, void *lib, const char *sym) {
  lua_CFunction f = cast_func(dlsym(lib, sym));
  if (f == NULL) lua_pushstring(L, dlerror());
  return f;
}

#endif

static int zll_lookforfunc(zll_State *S, lua_State *L, const char *path, const char *sym, mz_uint index) {
    lua_CFunction f;
    void *reg = zll_checkclib(S, L, path);
    if (reg == NULL) {
        reg = zll_load(S, L, path, index, *sym == '*');
        if (reg == NULL) return ZLL_ERRLIB;
        zll_addtoclib(S, L, path, reg);
    }
    if (*sym == '*') {  /* loading only library (no function)? */
        lua_pushboolean(L, 1);  /* return 'true' */
        return 0;  /* no errors */
    }
    if ((f = zll_sym(L, reg, sym)) == NULL)
        return ZLL_ERRFUNC;  /* unable to find function */
    lua_pushcfunction(L, f);  /* else create new function */
    return 0;  /* no errors */
}

static int Lsearcher_loadlib(lua_State *L) {
    zll_State *S = (zll_State*)luaL_checkudata(L, 1, ZLL_TYPE);
    const char *path = luaL_checkstring(L, 2);
    const char *init = luaL_checkstring(L, 3);
    int stat;
    mz_uint index;
    if (!mz_zip_reader_locate_file_v2(&S->ar, path, NULL, 0, &index))
        return luaL_error(L, "%s", zll_lasterror(S));
    if ((stat = zll_lookforfunc(S, L, path, init, index)) == 0)
        return 1;
    luaL_pushfail(L);
    lua_insert(L, -2);
    lua_pushstring(L, (stat == ZLL_ERRLIB) ?  "open" : "init");
    return 3;
}

/* basic operations */

static int zll_msghandler(lua_State *L) {
    const char *msg = lua_tostring(L, 1);
    if (msg == NULL) {
        if (luaL_callmeta(L, 1, "__tostring") &&
                lua_type(L, -1) == LUA_TSTRING)
            return 1;
        msg = lua_pushfstring(L, "(error object is a %s value)",
                luaL_typename(L, 1));
    }
#if LUA_VERSION_NUM > 501
    luaL_traceback(L, L, msg, 1);
#endif
    return 1;
}

static zll_State *zll_new(lua_State *L, const char *zipfile) {
    zll_State *S = (zll_State*)lua_newuserdata(L, sizeof(zll_State));
    memset(S, 0, sizeof(*S));
    mz_zip_zero_struct(&S->ar);
    luaL_setmetatable(L, ZLL_TYPE);
    S->filename = lua52_pushstring(L, zipfile ? zipfile : zll_binpath());
    S->fn_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_newtable(L);
    S->clibs_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    return S;
}

static int Lsearcher_entry(lua_State *L) {
    zll_State *S = (zll_State*)luaL_checkudata(L, 1, ZLL_TYPE);
    const char *script = luaL_optstring(L, 2, ZLL_ENTRY);
    mz_uint index;
    size_t len;
    const void *data;
    if (!mz_zip_reader_locate_file_v2(&S->ar, script, NULL, 0, &index)) {
        luaL_pushfail(L);
        lua_pushstring(L, zll_lasterror(S));
        return 2;
    }
    if (!(data = zll_filecontent(S, L, index, &len))) {
        luaL_pushfail(L);
        lua_insert(L, -2);
        return 2;
    }
    script = lua_pushfstring(L, "=%s", script);
    if (luaL_loadbuffer(L, data, len, script) != LUA_OK)
        lua_error(L);
    lua_call(L, 0, 0);
    return 0;
}

static int Lsearcher_new(lua_State *L) {
    const char *s = luaL_optstring(L, 1, 0);
    zll_State *S = s ? zll_new(L, s) : NULL;
    if (S == NULL) {
        luaL_getmetatable(L, ZLL_TYPE);
        if (lua53_getfield(L, -1, "self") == LUA_TUSERDATA)
            return 1;
        lua_pop(L, 1);
        S = zll_new(L, NULL);
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, "self");
        lua_pushcfunction(L, zll_msghandler);
        lua_pushcfunction(L, Lsearcher_entry);
        lua_pushvalue(L, -3);
        if (lua_pcall(L, 1, 0, -3) != LUA_OK)
            lua_error(L);
    }
    if (!mz_zip_reader_init_file(&S->ar, S->filename, 0)) {
        luaL_pushfail(L);
        lua_pushstring(L, zll_lasterror(S));
        return 2;
    }
    return 1;
}

static int Lsearcher_install(lua_State *L) {
    int ret = Lsearcher_new(L);
    if (ret == 1) zll_install(L);
    return ret;
}

static int Lsearcher_gc(lua_State *L) {
    zll_State *S = (zll_State*)luaL_testudata(L, 1, ZLL_TYPE);
    if (S != NULL) {
        mz_zip_reader_end(&S->ar);
        lua_rawgeti(L, LUA_REGISTRYINDEX, S->clibs_ref);
        lua_Integer n = luaL_len(L, -1);
        for (; n >= 1; n--) {
            lua_rawgeti(L, -1, (int)n);
            zll_unloadlib(lua_touserdata(L, -1));
            lua_pop(L, 1);
        }
        luaL_unref(L, LUA_REGISTRYINDEX, S->clibs_ref);
        luaL_unref(L, LUA_REGISTRYINDEX, S->fn_ref);
    }
    return 0;
}

static int Lsearcher_len(lua_State *L) {
    zll_State *S = (zll_State*)luaL_checkudata(L, 1, ZLL_TYPE);
    lua_pushinteger(L, mz_zip_reader_get_num_files(&S->ar));
    return 1;
}

static int Lsearcher_diraux(lua_State *L) {
    zll_State *S = (zll_State*)luaL_checkudata(L, 1, ZLL_TYPE);
    mz_zip_archive_file_stat fstat;
    lua_Integer i = luaL_optinteger(L, 2, -1);
    lua_Integer len = mz_zip_reader_get_num_files(&S->ar);
    while (++i < len) {
        if (!mz_zip_reader_file_stat(&S->ar, (mz_uint)i, &fstat))
            return luaL_error(L, "%s", zll_lasterror(S));
        if (!fstat.m_is_supported)
            continue;
        lua_pushinteger(L, i);
        lua_pushstring(L, fstat.m_filename);
        lua_pushstring(L, fstat.m_is_directory ? "dir" : "file");
        lua_pushinteger(L, (lua_Integer)fstat.m_uncomp_size);
        lua_pushinteger(L, (lua_Integer)fstat.m_comp_size);
        lua_pushinteger(L, fstat.m_crc32);
        return 6;
    }
    return 0;
}

static int Lsearcher_dir(lua_State *L) {
    lua_settop(L, 1);
    lua_pushcfunction(L, Lsearcher_diraux);
    lua_insert(L, -2);
    return 2;
}

static int Lsearcher_stat(lua_State *L) {
    zll_State *S = (zll_State*)luaL_checkudata(L, 1, ZLL_TYPE);
    mz_zip_archive_file_stat fstat;
    mz_uint fidx = zll_fileindex(S, L, 2);
    if (!mz_zip_reader_file_stat(&S->ar, fidx, &fstat))
        return luaL_error(L, "%s", zll_lasterror(S));
    lua_settop(L, 3);
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_createtable(L, 0, 20);
    }
    lua_pushinteger(L, fstat.m_file_index);        lua_setfield(L, -2, "file_index");
    lua_pushinteger(L, (lua_Integer)fstat.m_central_dir_ofs); lua_setfield(L, -2, "central_dir_ofs");
    lua_pushinteger(L, fstat.m_version_made_by);   lua_setfield(L, -2, "version_made_by");
    lua_pushinteger(L, fstat.m_version_needed);    lua_setfield(L, -2, "version_needed");
    lua_pushinteger(L, fstat.m_bit_flag);          lua_setfield(L, -2, "bit_flag");
    lua_pushinteger(L, fstat.m_method);            lua_setfield(L, -2, "method");
#ifndef MINIZ_NO_TIME
    lua_pushinteger(L, (lua_Integer)fstat.m_time); lua_setfield(L, -2, "time");
#endif
    lua_pushinteger(L, fstat.m_crc32);             lua_setfield(L, -2, "crc32");
    lua_pushinteger(L, (lua_Integer)fstat.m_comp_size); lua_setfield(L, -2, "comp_size");
    lua_pushinteger(L, (lua_Integer)fstat.m_uncomp_size); lua_setfield(L, -2, "uncomp_size");
    lua_pushinteger(L, fstat.m_internal_attr);     lua_setfield(L, -2, "internal_attr");
    lua_pushinteger(L, fstat.m_external_attr);     lua_setfield(L, -2, "external_attr");
    lua_pushinteger(L, (lua_Integer)fstat.m_local_header_ofs); lua_setfield(L, -2, "local_header_ofs");
    lua_pushboolean(L, fstat.m_is_directory);      lua_setfield(L, -2, "is_directory");
    lua_pushboolean(L, fstat.m_is_encrypted);      lua_setfield(L, -2, "is_encrypted");
    lua_pushboolean(L, fstat.m_is_supported);      lua_setfield(L, -2, "is_supported");
    lua_pushstring(L, fstat.m_filename);           lua_setfield(L, -2, "filename");
    lua_pushlstring(L, fstat.m_comment, fstat.m_comment_size);
    lua_setfield(L, -2, "comment");
    return 1;
}

static int Lsearcher_read(lua_State *L) {
    zll_State *S = (zll_State*)luaL_checkudata(L, 1, ZLL_TYPE);
    luaL_Buffer B;
    mz_uint fidx = zll_fileindex(S, L, 2);
    luaL_buffinit(L, &B);
    if (!mz_zip_reader_extract_to_callback(&S->ar, fidx, zll_file_write_func, &B, 0))
        return luaL_error(L, "%s", zll_lasterror(S));
    luaL_pushresult(&B);
    return 1;
}

/* search path */

typedef struct zll_SearchCtx {
    const char  *name;
    const char  *path;
    const char  *sep;
    const char  *dirsep;
    zll_State *S;
    mz_uint    index;
} zll_SearchCtx;

static const char *zll_getnextfilename(char **path, char *end) {
    char *sep, *name = *path;
    if (name == end) return NULL;
    if (*name == '\0') *name++ = *LUA_PATH_SEP;
    if ((sep = strchr(name, *LUA_PATH_SEP)) == NULL)
        sep = end;
    *sep = '\0';
    *path = sep;
    return name;
}

static void zll_pusherrornotfound(zll_State *S, lua_State *L, const char *path) {
    luaL_Buffer b;
    const char *fn = lua_pushfstring(L, "'\n\tno file '%s" LUA_DIRSEP, S->filename);
    const char *lp = luaL_gsub(L, path, "/", LUA_DIRSEP);
    luaL_buffinit(L, &b);
    luaL_addstring(&b, fn + 3);
    luaL_addgsub(&b, lp, LUA_PATH_SEP, fn);
    luaL_addstring(&b, "'");
    luaL_pushresult(&b);
}

static const char *zll_searchpath(lua_State *L, zll_SearchCtx *ctx) {
    luaL_Buffer B;
    char *pathname;
    char *endpathname;
    const char *filename;
    const char *name = ctx->name;
    if (*ctx->sep != '\0' && strchr(ctx->name, *ctx->sep) != NULL)
        ctx->name = luaL_gsub(L, ctx->name, ctx->sep, ctx->dirsep);
    luaL_buffinit(L, &B);
    luaL_addgsub(&B, ctx->path, LUA_PATH_MARK, ctx->name);
    luaL_addchar(&B, '\0');
    pathname = luaL_buffaddr(&B);
    endpathname = pathname + luaL_bufflen(&B) - 1;
    while ((filename = zll_getnextfilename(&pathname, endpathname)) != NULL)
        if (mz_zip_reader_locate_file_v2(&ctx->S->ar, filename, NULL, 0, &ctx->index))
            return lua52_pushstring(L, filename);
    luaL_pushresult(&B);
    if (name != ctx->name) ctx->name = name;
    zll_pusherrornotfound(ctx->S, L, lua_tostring(L, -1));
    return NULL;
}

static int Lsearcher_searchpath(lua_State *L) {
    zll_State *S = (zll_State*)luaL_checkudata(L, 1, ZLL_TYPE);
    zll_SearchCtx ctx;
    const char *f;
    ctx.name   = luaL_checkstring(L, 2);
    ctx.path   = luaL_checkstring(L, 3);
    ctx.sep    = luaL_optstring(L, 4, ".");
    ctx.dirsep = luaL_optstring(L, 5, "/");
    ctx.S     = S;
    if ((f = zll_searchpath(L, &ctx)) != NULL) {
        lua_pushinteger(L, ctx.index);
        return 2;
    }
    luaL_pushfail(L);
    lua_insert(L, -2);
    return 2;
}

/* loader routines */

#define ZLL_IGMARK    "-"
#define ZLL_POF		    "luaopen_"
#define ZLL_OFSEP	    "_"

static int zll_loadfunc(zll_State *S, lua_State *L, const char *modname, const char *filename, mz_uint index) {
    const char *openfunc;
    const char *mark;
    modname = luaL_gsub(L, modname, ".", ZLL_OFSEP);
    mark = strchr(modname, *ZLL_IGMARK);
    if (mark) {
        int stat;
        openfunc = lua52_pushlstring(L, modname, mark - modname);
        openfunc = lua52_pushfstring(L, ZLL_POF"%s", openfunc);
        stat = zll_lookforfunc(S, L, filename, openfunc, index);
        if (stat != ZLL_ERRFUNC) return stat;
        modname = mark + 1;  /* else go ahead and try old-style name */
    }
    openfunc = lua_pushfstring(L, ZLL_POF"%s", modname);
    return zll_lookforfunc(S, L, filename, openfunc, index);
}

static const char *zll_findfile(zll_State *S, lua_State *L, int idx, const char *pname, const char *name, mz_uint *pindex) {
    zll_SearchCtx ctx;
    const char *ret;
    ctx.name   = name;
    ctx.sep    = ".";
    ctx.dirsep = "/";
    ctx.S      = S;
    lua_getfield(L, idx, pname);
    if ((ctx.path  = lua_tostring(L, -1)) == NULL)
        luaL_error(L, "zll_State.'%s' must be a string", pname);
    ret = zll_searchpath(L, &ctx);
    if (pindex) *pindex = ctx.index;
    return ret;
}

static int zll_checkload(zll_State *S, lua_State *L, int loadok, const char *fn) {
    if (!loadok)
        return luaL_error(L, "error loading module '%s' from file '%s':\n\t%s",
                lua_tostring(L, 2), fn, lua_tostring(L, -1));
    lua_pushfstring(L, "%s/%s", S->filename, fn);
    return 2;
}

static int zll_searcher_Lua(lua_State *L) {
    zll_State *S = (zll_State*)luaL_checkudata(L, 1, ZLL_TYPE);
    const char *name = luaL_checkstring(L, 2), *fn;
    mz_uint index;
    if ((fn = zll_findfile(S, L, 1, "path", name, &index))) { /* try Lua code */
        size_t len;
        const void *data = zll_filecontent(S, L, index, &len);
        const char *name = lua_pushfstring(L, "=%s/%s", S->filename, fn);
        return zll_checkload(S, L, luaL_loadbuffer(L, data, len, name) == LUA_OK, fn);
    }
    return 1;
}

static int zll_searcher_C(lua_State *L) {
    zll_State *S = (zll_State*)luaL_checkudata(L, 1, ZLL_TYPE);
    const char *name = luaL_checkstring(L, 2), *fn;
    mz_uint index;
    if ((fn = zll_findfile(S, L, 1, "cpath", name, &index))) /* try DLL module */
        return zll_checkload(S, L, zll_loadfunc(S, L, name, fn, index) == 0, fn);
    return 1;
}

static int zll_searcher_Croot(lua_State *L) {
    zll_State *S = (zll_State*)luaL_checkudata(L, 1, ZLL_TYPE);
    const char *name = luaL_checkstring(L, 2), *fn, *p;
    mz_uint index;
    if ((p = strchr(name, '.')) == NULL) return 0; /* is root */
    fn = zll_findfile(S, L, 1, "cpath",
            lua52_pushlstring(L, name, p - name), &index);
    lua_remove(L, -2);
    if (fn) {
        int stat = zll_loadfunc(S, L, name, fn, index);
        if (stat == ZLL_ERRFUNC) {
            lua_pop(L, 1);
            lua_pushfstring(L, "no module '%s' in file '%s'", name, fn);
        }
        return zll_checkload(S, L, stat == 0, fn);
    }
    return 1;
}

static int Lsearcher_call(lua_State *L) {
    luaL_Reg searchers[] = {
        { "Lua",   zll_searcher_Lua   },
        { "C",     zll_searcher_C     },
        { "Croot", zll_searcher_Croot },
        { NULL, NULL }
    }, *p = searchers;
    luaL_Buffer B;  /* to build error message */
    lua_settop(L, 2);
    luaL_buffinit(L, &B);
    for (; p->name != NULL; ++p) {
        if (p != searchers) luaL_addstring(&B, "\n\t");
        lua_pushcfunction(L, p->func);
        lua_pushvalue(L, 1);
        lua_pushvalue(L, 2);
        lua_call(L, 2, 2);
        if (lua_isfunction(L, -2))
            return 2;
        else if (lua_isstring(L, -2)) {
            lua_pop(L, 1);
            luaL_addvalue(&B);
        } else {
            lua_pop(L, 2);
            luaL_buffsub(&B, 2);
        }
    }
    return luaL_pushresult(&B), 1;
}

/* module entry */

LUALIB_API int luaopen_ziploader(lua_State *L) {
    luaL_Reg libs[] = {
#define ENTRY(name) { #name, Lsearcher_ ## name }
        { "__index", NULL           },
        { "self",    NULL           },
        { "path",    NULL           },
        { "cpath",   NULL           },
        { "__gc",    Lsearcher_gc   },
        { "__call",  Lsearcher_call },
        { "__len",   Lsearcher_len  },
        ENTRY(new),
        ENTRY(install),
        ENTRY(entry),
        ENTRY(read),
        ENTRY(dir),
        ENTRY(stat),
        ENTRY(loadlib),
        ENTRY(searchpath),
#undef  ENTRY
        { NULL, NULL }
    };
    if (luaL_newmetatable(L, ZLL_TYPE)) {
        luaL_setfuncs(L, libs, 0);
        lua_pushstring(L, ZLL_PATH); lua_setfield(L, -2, "path");
        lua_pushstring(L, ZLL_CPATH); lua_setfield(L, -2, "cpath");
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, Lsearcher_install);
        lua_call(L, 0, 0);
    }
    return 1;
}

/* win32cc: flags+='-s -O2 -mdll -DLUA_BUILD_AS_DLL --coverage' libs+="-llua54"
 * win32cc: input+='miniz.c' output='ziploader.dll'
 * maccc: flags+='-O2 -shared -undefined dynamic_lookup'
 * maccc: input+='miniz.c' output='ziploader.so'
 */

