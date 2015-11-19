#include "luabuild.h"
#include "lpath.c"

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <stdarg.h>


static int file_exists(const char *s) {
    FILE *fp = fopen(s, "r");
    if (fp == NULL) return 0;
    fclose(fp);
    return 1;
}

static int report(lua_State *L, int status) {
  if (status != LUA_OK) {
    fprintf(stderr, "luabuild: %s\n", lua_tostring(L, -1));
    lua_pop(L, 1);  /* remove message */
  }
  return status == LUA_OK;
}

static int pmain(lua_State *L) {
    int external;
    luaL_openlibs(L);
    lua_getfield(L, LUA_REGISTRYINDEX, "_LOADED");
    luaopen_path(L);
    lua_setfield(L, -2, "path");
    lua_pop(L, 1);

    /* setup path module */
    lua_getfield(L, LUA_REGISTRYINDEX, "_LOADED");
    lua_pushcfunction(L, luaopen_path);
    lua_setfield(L, -2, "path");
    lua_pop(L, 1);

    /* collect arguments */
    {
        int i, argc = lua_tointeger(L, 1);
        char** argv = (char**)lua_touserdata(L, 2);
        lua_createtable(L, argc, 0);
        for (i = 0; i < argc; ++i) {
            lua_pushstring(L, argv[i]);
            lua_rawseti(L, -2, i);
        }
        lua_setglobal(L, "arg");
    }

    /* get debug.traceback() */
    lua_getglobal(L, "debug");
    lua_getfield(L, -1, "traceback");

    if (!(external = file_exists("luabuild.lua")) ||
            !report(L, luaL_loadfile(L, "luabuild.lua")) ||
            !report(L, lua_pcall(L, 0, 0, -2))) {
        if (external)
            fprintf(stderr, "\n\n[ERROR] use external script fail, "
                    "use internal one ...\n\n");
        if (!report(L, load_chunk(L)) ||
                !report(L, lua_pcall(L, 0, 0, -2)))
            return 0;
    }

    lua_pushboolean(L, 1);
    return 1;
}

int main(int argc, char **argv) {
    int status, result;
    lua_State *L = luaL_newstate();  /* create state */
    if (L == NULL) {
        fprintf(stderr, "%s: cannot create state: not enough memory", argv[0]);
        return EXIT_FAILURE;
    }
    lua_pushcfunction(L, &pmain);  /* to call 'pmain' in protected mode */
    lua_pushinteger(L, argc);  /* 1st argument */
    lua_pushlightuserdata(L, argv); /* 2nd argument */
    status = lua_pcall(L, 2, 1, 0);  /* do the call */
    result = lua_toboolean(L, -1);  /* get result */
    report(L, status);
    lua_close(L);
    if (result && status == LUA_OK)
        return EXIT_SUCCESS;
    system("pause");
    return EXIT_FAILURE;
}
/* cc: libs+='-static -llua53' */
