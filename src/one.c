/*
* one.c -- Lua core, libraries, and interpreter in a single file
*/

/* default is to build the full interpreter */
#if !defined(MAKE_LIB) && !defined(MAKE_LUA) && !defined(MAKE_LUAC)
# define MAKE_LUA
#endif

#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
# define  _CRT_SECURE_NO_WARNINGS
#endif

/* choose suitable platform-specific features */
/* some of these may need extra libraries such as -ldl -lreadline -lncurses */
#if 0
#define LUA_USE_LINUX
#define LUA_USE_MACOSX
#define LUA_USE_POSIX
#define LUA_USE_DLOPEN
#define LUA_USE_READLINE
#define LUA_ANSI
#define LUA_32BITS
#define LUA_USE_C89
#define LUA_C89_NUMBERS
#endif

/* no need to change anything below this line ----------------------------- */

/* setup for luaconf.h */
#if HAVE_LPREFIX
/* activate system definitions in lprefix.h */
#include "lprefix.h"
#endif

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


/* setup for luaconf.h */
#define LUA_CORE
#define LUA_LIB
#define ltable_c
#if LUA_VERSION_NUM < 504
#define loslib_c
#endif
#define lvm_c
#include "luaconf.h"
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
#include "lzio.c"
#if LUA_VERSION_NUM > 501
#include "lctype.c"
#endif
#include "lopcodes.c"
#include "lmem.c"
#include "lundump.c"
#include "ldump.c"
#include "lstate.c"
#include "lgc.c"
#include "llex.c"
#include "lcode.c"
#include "lparser.c"
#include "ldebug.c"
#include "lfunc.c"
#include "lobject.c"
#include "ltm.c"
#include "lstring.c"
#include "ltable.c"
#include "ldo.c"
#include "lvm.c"
#include "lapi.c"
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
#if LUA_VERSION_NUM >= 502
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
#include "lutf8lib.c"
#endif
#include "linit.c"
#include "lpath.c"
#include "lminiz.c"
#include "lfmt.c"
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

