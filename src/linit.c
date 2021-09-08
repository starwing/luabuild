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

#ifdef HAVE_PREFIX_H
#include "lprefix.h"
#endif

#ifndef LUA_GNAME
#define LUA_GNAME "_G"
#endif


#include <stddef.h>
#include <string.h>

#include "lua.h"

#include "lualib.h"
#include "lauxlib.h"

#if LUA_VERSION_NUM == 501
# define LUAMOD_API LUA_API
#endif

LUAMOD_API int luaopen_path(lua_State *L);
LUAMOD_API int luaopen_path_fs(lua_State *L);
LUAMOD_API int luaopen_path_info(lua_State *L);
LUAMOD_API int luaopen_path_env(lua_State *L);
LUAMOD_API int luaopen_miniz(lua_State *L);
LUAMOD_API int luaopen_fmt(lua_State *L);
LUAMOD_API int luaopen_mp(lua_State *L);
LUAMOD_API int luaopen_ziploader(lua_State *L);

/*
** these libs are loaded by lua.c and are readily available to any Lua
** program
*/
static const luaL_Reg loadedlibs[] = {
  {LUA_GNAME, luaopen_base},
  {LUA_LOADLIBNAME, luaopen_package},
#if LUA_VERSION_NUM >= 502
  {LUA_COLIBNAME, luaopen_coroutine},
#endif
  {LUA_TABLIBNAME, luaopen_table},
  {LUA_IOLIBNAME, luaopen_io},
  {LUA_OSLIBNAME, luaopen_os},
  {LUA_STRLIBNAME, luaopen_string},
  {LUA_MATHLIBNAME, luaopen_math},
#if LUA_VERSION_NUM >= 503
  {LUA_UTF8LIBNAME, luaopen_utf8},
#endif
  {LUA_DBLIBNAME, luaopen_debug},
#if LUA_VERSION_NUM == 502 || defined(LUA_COMPAT_BITLIB)
  {LUA_BITLIBNAME, luaopen_bit32},
#endif
  {NULL, NULL}
};

static int builtinlibs(lua_State *L) {
  luaL_Reg *lib, extlibs[] = {
    { "path",      luaopen_path      },
    { "path.fs",   luaopen_path_fs   },
    { "path.info", luaopen_path_info },
    { "path.env",  luaopen_path_env  },
    { "fs",        luaopen_path_fs   },
    { "env",       luaopen_path_env  },
    { "miniz",     luaopen_miniz     },
    { "fmt",       luaopen_fmt       },
    { "mp",        luaopen_mp        },
#if LUA_VERSION_NUM >= 503
    { "ziploader", luaopen_ziploader },
#endif
    { NULL, NULL }
  };
  const char *libname = luaL_checkstring(L, 1);
  for (lib = extlibs; lib->func; lib++) {
    if (strcmp(libname, lib->name) == 0) {
#if LUA_VERSION_NUM >= 502
      luaL_requiref(L, lib->name, lib->func, 0);
#else
      lua_pushcfunction(L, lib->func);
      lua_pushstring(L, lib->name);
      lua_call(L, 1, 1);
      if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_pushboolean(L, 1);
      }
      lua_getfield(L, LUA_REGISTRYINDEX, "_LOADED");
      lua_pushstring(L, lib->name);
      lua_pushvalue(L, -3);
      lua_rawset(L, -3);
      lua_pop(L, 1);
#endif
      return 1;
    }
  }
  return 0;
}

LUALIB_API void luaL_openlibs (lua_State *L) {
  const luaL_Reg *lib;
  /* "require" functions from 'loadedlibs' and set results to global table */
  for (lib = loadedlibs; lib->func; lib++) {
#if LUA_VERSION_NUM >= 502
    luaL_requiref(L, lib->name, lib->func, 1);
    lua_pop(L, 1);  /* remove lib */
#else
    lua_pushcfunction(L, lib->func);
    lua_pushstring(L, lib->name);
    lua_call(L, 1, 0);
#endif
  }
  lua_pushcfunction(L, builtinlibs);
#if LUA_VERSION_NUM >= 503
  lua_pushvalue(L, -1);
  lua_pushliteral(L, "ziploader");
  lua_call(L, 1, 0);
#endif
  lua_setglobal(L, "builtin");
}

