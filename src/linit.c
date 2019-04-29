/*
** $Id: linit.c,v 1.39 2016/12/04 20:17:24 roberto Exp $
** Initialization of libraries for lua.c and other clients
** See Copyright Notice in lua.h
*/


#define linit_c
#define LUA_LIB

/*
** If you embed Lua in your program and need to open the standard
** libraries, call luaL_openlibs in your program. If you need a
** different set of libraries, copy this file to your project and edit
** it to suit your needs.
**
** You can also *preload* libraries, so that a later 'require' can
** open the library, which is already linked to the application.
** For that, do the following code:
**
**  luaL_getsubtable(L, LUA_REGISTRYINDEX, LUA_PRELOAD_TABLE);
**  lua_pushcfunction(L, luaopen_modname);
**  lua_setfield(L, -2, modname);
**  lua_pop(L, 1);  // remove PRELOAD table
*/

#include "lua.h"

#include "lualib.h"
#include "lauxlib.h"

LUAMOD_API int luaopen_path(lua_State *L);
LUAMOD_API int luaopen_path_fs(lua_State *L);
LUAMOD_API int luaopen_path_info(lua_State *L);
LUAMOD_API int luaopen_miniz(lua_State *L);

/*
** these libs are loaded by lua.c and are readily available to any Lua
** program
*/
static const luaL_Reg loadedlibs[] = {
  {"_G", luaopen_base},
  {LUA_LOADLIBNAME, luaopen_package},
  {LUA_COLIBNAME, luaopen_coroutine},
  {LUA_TABLIBNAME, luaopen_table},
  {LUA_IOLIBNAME, luaopen_io},
  {LUA_OSLIBNAME, luaopen_os},
  {LUA_STRLIBNAME, luaopen_string},
  {LUA_MATHLIBNAME, luaopen_math},
  {LUA_UTF8LIBNAME, luaopen_utf8},
  {LUA_DBLIBNAME, luaopen_debug},
#if LUA_VERSION_NUM == 502 || defined(LUA_COMPAT_BITLIB)
  {LUA_BITLIBNAME, luaopen_bit32},
#endif
  {NULL, NULL}
};

static int extlibs(lua_State *L) {
    luaL_Reg *lib, extlibs[] = {
        {"path", luaopen_path},
        {"path.fs", luaopen_path_fs},
        {"path.info", luaopen_path_info},
        {"fs", luaopen_path_fs},
        {"miniz", luaopen_miniz},
        {NULL, NULL}
    };
    const char *libname = luaL_checkstring(L, 1);
    for (lib = extlibs; lib->func; lib++) {
        if (strcmp(libname, lib->name) == 0) {
            luaL_requiref(L, lib->name, lib->func, 0);
            return 1;
        }
    }
    return 0;
}

LUALIB_API void luaL_openlibs (lua_State *L) {
  const luaL_Reg *lib;
  /* "require" functions from 'loadedlibs' and set results to global table */
  for (lib = loadedlibs; lib->func; lib++) {
    luaL_requiref(L, lib->name, lib->func, 1);
    lua_pop(L, 1);  /* remove lib */
  }
  lua_pushcfunction(L, extlibs);
  lua_setglobal(L, "ext");
}

