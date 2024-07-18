#define LUA_LIB
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <string.h>

#define LB64_STD "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/="
#define LB64_URL "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_="

static int Lb64_encode(lua_State *L) {
    size_t len, elen;
    const char *s = luaL_checklstring(L, 1, &len);
    const char *enc = luaL_optlstring(L, 2, LB64_STD, &elen);
    size_t i, chunk = len/3, remain = len%3;
    luaL_Buffer B;
    luaL_argcheck(L, (elen == 64 || elen == 65),
            2, "invalid encoding length");
    if (len == 0) return (lua_pushliteral(L, ""), 1);
    luaL_buffinit(L, &B);
    for (i = 0; i < chunk; ++i) {
        unsigned buff = (s[0] & 0xFF) << 16
                      | (s[1] & 0xFF) << 8
                      | (s[2] & 0xFF);
        s += 3;
        luaL_addchar(&B, enc[(buff >> 18) & 63]);
        luaL_addchar(&B, enc[(buff >> 12) & 63]);
        luaL_addchar(&B, enc[(buff >> 6)  & 63]);
        luaL_addchar(&B, enc[buff         & 63]);
    }
    if (remain) {
        unsigned buff = (s[0] & 0xFF) << 16
                      | (remain == 2 ? s[1] & 0xFF : 0);
        luaL_addchar(&B, enc[(buff >> 18) & 63]);
        luaL_addchar(&B, enc[(buff >> 12) & 63]);
        if (remain == 2) luaL_addchar(&B, enc[(buff >> 6) & 63]);
        if (elen == 65) {
            if (remain == 1) luaL_addchar(&B, enc[64]);
            luaL_addchar(&B, enc[64]);
        }
    }
    return (luaL_pushresult(&B), 1);
}

static unsigned lb64_checkchunk(lua_State *L, const char *s, char a, char b, char c, char d) {
    if (((a >> 6) | (b >> 6) | (c >> 6) | (d >> 6)) != 0) {
        int idx  = (a >> 6) ? 0 : (b >> 6) ? 1 : (c >> 6) ? 2 : 3;
        lua_pushfstring(L, "invalid chunk '%c'", s[idx]);
        return (unsigned)luaL_argerror(L, 1, lua_tostring(L, -1));
    }
    return a << 18 | b << 12 | c << 6 | d;
}

static int Lb64_decode(lua_State *L) {
    size_t len, elen;
    const char *s = luaL_checklstring(L, 1, &len);
    const char *enc = luaL_optlstring(L, 2, LB64_STD, &elen);
    size_t i, chunk = len/4, remain = len%4;
    char dec[256];
    luaL_Buffer B;
    luaL_argcheck(L, (elen == 64 || elen == 65),
            2, "invalid encoding length");
    if (len == 0) return (lua_pushliteral(L, ""), 1);
    luaL_argcheck(L, (remain != 1),
            1, "invalid base64 data length");
    luaL_argcheck(L, (elen == 64 || remain == 0),
            1, "invalid base64 data padding");
    luaL_argcheck(L, (s[len-2] != enc[64] || s[len-1] == enc[64]),
            1, "invalid base64 data padding");
    memset(dec, 0xFF, 256);
    for (i = 0; i < 64; ++i)
        dec[enc[i] & 0xFF] = (char)i;
    luaL_buffinit(L, &B);
    for (i = !remain; i < chunk; ++i) {
        char a = dec[s[0] & 0xFF], b = dec[s[1] & 0xFF];
        char c = dec[s[2] & 0xFF], d = dec[s[3] & 0xFF];
        unsigned buff = lb64_checkchunk(L, s, a, b, c, d);
        s += 4;
        luaL_addchar(&B, (buff >> 16) & 0xFF);
        luaL_addchar(&B, (buff >>  8) & 0xFF);
        luaL_addchar(&B, (buff      ) & 0xFF);
    }
    if (elen == 65) dec[enc[64] & 0xFF] = 0;
    {
        char a = dec[s[0] & 0xFF], b = dec[s[1] & 0xFF];
        char c = remain != 2 ? dec[s[2] & 0xFF] : 0;
        char d = remain == 0 ? dec[s[3] & 0xFF] : 0;
        unsigned buff = lb64_checkchunk(L, s, a, b, c, d);
        if (elen == 65) {
            if (s[3] == enc[64]) remain = 3;
            if (s[2] == enc[64]) remain = 2;
        }
        luaL_addchar(&B, (buff >> 16) & 0xFF);
        if (remain != 2) luaL_addchar(&B, (buff >>  8) & 0xFF);
        if (remain == 0) luaL_addchar(&B, (buff      ) & 0xFF);
    }
    return (luaL_pushresult(&B), 1);
}

LUALIB_API int luaopen_base64(lua_State *L) {
    luaL_Reg libs[] = {
#define ENTRY(name) { #name, Lb64_##name }
        ENTRY(encode),
        ENTRY(decode),
#undef  ENTRY
        { NULL, NULL }
    };
    luaL_newlib(L, libs);
    lua_pushliteral(L, LB64_STD);
    lua_setfield(L, -2, "stdencoding");
    lua_pushliteral(L, LB64_URL);
    lua_setfield(L, -2, "urlencoding");
    return 1;
}

/* cc: flags+='-s -O2 -mdll -DLUA_BUILD_AS_DLL'
 * cc: libs+='-llua54' output='base64.dll' */

