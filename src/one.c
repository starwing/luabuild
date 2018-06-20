/*
* one.c -- Lua core, libraries, and interpreter in a single file
*/

/* default is to build the full interpreter */
#if !defined(MAKE_LIB) && !defined(MAKE_LUA) && !defined(MAKE_LUAC)
# define MAKE_LUA
#endif

/* choose suitable platform-specific features */
/* some of these may need extra libraries such as -ldl -lreadline -lncurses */
//#define LUA_USE_LINUX
//#define LUA_USE_MACOSX
//#define LUA_USE_POSIX
//#define LUA_ANSI

/* no need to change anything below this line ----------------------------- */

/* setup for luaconf.h */
#if HAVE_LPREFIX
# include "lprefix.h"
#endif
#define LUA_CORE
#define LUA_LIB
#define ltable_c
#define lvm_c
#define loslib_c
#include "lua.h"

/* do not export internal symbols */
#undef LUAI_FUNC
#undef LUAI_DDEC
#undef LUAI_DDEF
#define LUAI_FUNC	static
#define LUAI_DDEF	static

#if LUA_VERSION_NUM >= 504
# define LUAI_DDEC(def)	static def
#else
# define LUAI_DDEC	static
#endif

/* core -- used by all */
#include "lapi.c"
#include "lcode.c"
#if LUA_VERSION_NUM > 501
#include "lctype.c"
#endif
#include "ldebug.c"
#include "ldo.c"
#include "ldump.c"
#include "lfunc.c"
#include "lgc.c"
#include "llex.c"
#include "lmem.c"
#include "lobject.c"
#if LUA_VERSION_NUM >= 504 && !defined(MAKE_LUAC)
# include "lopcodes.c"
#endif
#include "lparser.c"
#include "lstate.c"
#include "lstring.c"
#include "ltable.c"
#include "ltm.c"
#include "lundump.c"
#include "lvm.c"
#include "lzio.c"
#if LUA_VERSION_NUM == 501 && defined(MAKE_LUAC)
#include "print.c"
#endif

/* auxiliary library -- used by all */
#include "lauxlib.c"

/* standard library  -- not used by luac */
#ifndef MAKE_LUAC
#include "lbaselib.c"
#if LUA_VERSION_NUM == 502
# include "lbitlib.c"
#endif
#if LUA_VERSION_NUM >= 501
#include "lcorolib.c"
#endif
#include "ldblib.c"
#include "liolib.c"
#include "lmathlib.c"
#include "loadlib.c"
#include "loslib.c"
#include "lstrlib.c"
#include "ltablib.c"
#if LUA_VERSION_NUM >= 503
# include "lutf8lib.c"
#endif
#include "linit.c"
#include "lpath.c"
#include "lminiz.c"
#endif

/* lua */
#ifdef MAKE_LUA
#include "lua.c"
#endif

/* luac */
#ifdef MAKE_LUAC
#include "luac.c"
#endif

/* cc: flags+='-Wextra -Wno-strict-aliasing -std=gnu99 -O4 -I../../src'
 * cc: flags+='-DLUA_COMPAT_5_3 -DLUA_BUILD_AS_DLL -DHAVE_LPREFIX -DMAKE_LUA' */

