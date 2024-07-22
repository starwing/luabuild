#define LUA_LIB
#include <lua.h>
#include <lauxlib.h>

#ifndef luaL_pushfail
# define luaL_pushfail lua_pushnil
#endif

#ifndef lua_relindex
#define lua_relindex lua_relindex
static int lua_relindex(int idx, int onstack) {
    return idx >= 0 || idx <= LUA_REGISTRYINDEX ?
        idx : idx - onstack;
}
#endif

#if LUA_VERSION_NUM >= 503
# define lua53_getfield   lua_getfield
# define lua53_geti       lua_geti
#else
# ifndef lua53_getfield
#   define lua53_getfield lua53_getfield
static int lua53_getfield(lua_State *L, int idx, const char *f)
{ return (lua_getfield(L, idx, f), lua_type(L, -1)); }
# endif
# ifndef lua53_geti
#   define lua53_geti lua53_geti
static int lua53_geti(lua_State *L, int idx, int i)
{ return (lua_pushinteger(L, i),
        lua_gettable(L, lua_relindex(idx, 1)), lua_type(L, -1)); }
# endif
#endif

#if LUA_VERSION_NUM < 503
# ifndef lua_geti
#   define lua_geti lua_geti
static void lua_geti(lua_State *L, int idx, int i)
{ lua_pushinteger(L, i); lua_gettable(L, idx); }
# endif

# ifndef lua_isinteger
#   define lua_isinteger lua_isinteger
static int lua_isinteger(lua_State *L, int idx) {
    lua_Number v = lua_tonumber(L, idx);
    if (v == 0.0 && lua_type(L,idx) != LUA_TNUMBER) return 0;
    return (lua_Number)(lua_Integer)v == v;
}
# endif
#endif

#define YYJSON_DISABLE_UTILS 1
#define yyjson_api yyjson_api_inline
#define read_string lyyjson_read_string
#define read_number lyyjson_read_number
#include "yyjson.h"
#include "yyjson.c"
#undef read_string
#undef read_number

#define LYYJSON_OBJ_TYPE   "yyjson.Object"
#define LYYJSON_ARR_TYPE   "yyjson.Array"
#define LYYJSON_TYPE_FIELD "__jsontype"
#define LYYJSON_TYPE_ARR   "array"
#define LYYJSON_TYPE_OBJ   "object"

static yyjson_read_flag ljson_checkreadflags(lua_State *L, int idx) {
    const char *opts[] = {
        "stop_when_done",
        "allow_trailing_commas",
        "allow_comments",
        "allow_inf_and_nan",
        "number_as_raw",
        "allow_invalid_unicode",
        "bignum_as_raw",
        NULL
    };
    yyjson_read_flag flags[] = {
        YYJSON_READ_STOP_WHEN_DONE,
        YYJSON_READ_ALLOW_TRAILING_COMMAS,
        YYJSON_READ_ALLOW_COMMENTS,
        YYJSON_READ_ALLOW_INF_AND_NAN,
        YYJSON_READ_NUMBER_AS_RAW,
        YYJSON_READ_ALLOW_INVALID_UNICODE,
        YYJSON_READ_BIGNUM_AS_RAW,
    };
    int i, top = lua_gettop(L);
    yyjson_read_flag r = 0;
    for (i = idx; i <= top; ++i) {
        int opt = luaL_checkoption(L, idx, NULL, opts);
        r |= flags[opt];
    }
    return r;
}

static int ljson_decodeval(lua_State *L, yyjson_val *val) {
    size_t idx, max;
    yyjson_val *k, *v;
    switch (unsafe_yyjson_get_type(val)) {
    case YYJSON_TYPE_NULL:
        return (lua_pushnil(L), 1);
    case YYJSON_TYPE_BOOL:
        return (lua_pushboolean(L, unsafe_yyjson_is_true(val)), 1);
    case YYJSON_TYPE_NUM:
        if (unsafe_yyjson_is_real(val))
            return (lua_pushnumber(L, unsafe_yyjson_get_real(val)), 1);
        return (lua_pushinteger(L,
                    (lua_Integer)unsafe_yyjson_get_uint(val)), 1);
    case YYJSON_TYPE_STR:
        return lua_pushlstring(L,
                unsafe_yyjson_get_str(val), unsafe_yyjson_get_len(val)), 1;
    case YYJSON_TYPE_ARR:
        luaL_checkstack(L, 2, "json object level too deep");
        lua_createtable(L, (int)unsafe_yyjson_get_len(val), 0);
        luaL_setmetatable(L, LYYJSON_ARR_TYPE);
        yyjson_arr_foreach(val, idx, max, v) {
            ljson_decodeval(L, v);
            lua_rawseti(L, -2, idx+1);
        }
        break;
    case YYJSON_TYPE_OBJ:
        luaL_checkstack(L, 3, "json object level too deep");
        lua_createtable(L, 0, (int)unsafe_yyjson_get_len(val));
        luaL_setmetatable(L, LYYJSON_OBJ_TYPE);
        yyjson_obj_foreach(val, idx, max, k, v) {
            ljson_decodeval(L, k);
            ljson_decodeval(L, v);
            lua_rawset(L, -3);
        }
        break;
    default:
        lua_pushfstring(L, "invalid json type: %d", unsafe_yyjson_get_tag(val));
        return luaL_error(L, lua_tostring(L, -1));
    }
    return 1;
}

static int ljson_decoder(lua_State *L) {
    yyjson_doc *doc = (yyjson_doc*)lua_touserdata(L, 1);
    yyjson_val *val = yyjson_doc_get_root(doc);
    if (val == NULL) luaL_error(L, "not root for json document");
    return ljson_decodeval(L, val);
}

static int Ljson_decode(lua_State *L) {
    size_t len;
    const char *s = luaL_checklstring(L, 1, &len);
    yyjson_read_flag flag = ljson_checkreadflags(L, 2);
    yyjson_read_err err;
    yyjson_doc *doc = yyjson_read_opts((char*)s, len, flag, NULL, &err);
    int r;
    if (doc == NULL) {
        luaL_pushfail(L);
        lua_pushstring(L, err.msg);
        lua_pushinteger(L, err.code);
        lua_pushinteger(L, (lua_Integer)err.pos);
        return 4;
    }
    lua_pushcfunction(L, ljson_decoder);
    lua_pushlightuserdata(L, doc);
    if ((r = lua_pcall(L, 1, 1, 0)) == LUA_OK)
        lua_pushinteger(L, yyjson_doc_get_read_size(doc));
    yyjson_doc_free(doc);
    if (r != LUA_OK) {
        luaL_pushfail(L);
        lua_pushvalue(L, -2);
    }
    return 2;
}

static void ljson_encodeval(lua_State *L, int idx, yyjson_mut_doc *doc, yyjson_mut_val *val);

static yyjson_write_flag ljson_checkwriteflags(lua_State *L, int idx) {
    const char *opts[] = {
        "pretty",
        "escape_unicode",
        "escape_slashes",
        "allow_inf_and_nan",
        "inf_and_nan_as_null",
        "allow_invalid_unicode",
        "pretty_two_spaces",
        "newline_at_end",
        NULL
    };
    yyjson_write_flag flags[] = {
        YYJSON_WRITE_PRETTY,
        YYJSON_WRITE_ESCAPE_UNICODE,
        YYJSON_WRITE_ESCAPE_SLASHES,
        YYJSON_WRITE_ALLOW_INF_AND_NAN,
        YYJSON_WRITE_INF_AND_NAN_AS_NULL,
        YYJSON_WRITE_ALLOW_INVALID_UNICODE,
        YYJSON_WRITE_PRETTY_TWO_SPACES,
        YYJSON_WRITE_NEWLINE_AT_END,
    };
    int i, top = lua_gettop(L);
    yyjson_write_flag r = 0;
    for (i = idx; i <= top; ++i) {
        int opt = luaL_checkoption(L, idx, NULL, opts);
        r |= flags[opt];
    }
    return r;
}

static int ljson_isarray(lua_State *L, int idx) {
    if (lua_getmetatable(L, idx)) {
        if (lua53_getfield(L, -1, LYYJSON_TYPE_FIELD) == LUA_TSTRING) {
            const char *s = lua_tostring(L, -1);
            if (*s == 'a' || *s == 'A') return lua_pop(L, 2), 1;
            if (*s == 'o' || *s == 'O') return lua_pop(L, 2), 0;
        }
        lua_pop(L, 2);
    }
    return luaL_len(L, idx) > 0;
}

static void ljson_encodeobj(lua_State *L, int idx, yyjson_mut_doc *doc, yyjson_mut_val *val) {
    if (ljson_isarray(L, idx)) {
        size_t i;
        luaL_checkstack(L, 1, "lua table too complex or has loop");
        unsafe_yyjson_set_arr(val, 0);
        for (i = 1; lua53_geti(L, idx, i) != LUA_TNIL; ++i) {
            yyjson_mut_val *v = unsafe_yyjson_mut_val(doc, 1);
            ljson_encodeval(L, -1, doc, v);
            yyjson_mut_arr_append(val, v);
            lua_pop(L, 1);
        }
        lua_pop(L, 1);
    } else {
        size_t vlen = 0;
        luaL_checkstack(L, 2, "lua table too complex or has loop");
        unsafe_yyjson_set_obj(val, vlen);
        lua_pushnil(L);
        while (lua_next(L, lua_relindex(idx, 1))) {
            size_t len;
            const char *s = lua_tolstring(L, -2, &len);
            if (s != NULL) {
                yyjson_mut_val *kv = unsafe_yyjson_mut_val(doc, 2);
                unsafe_yyjson_set_strn(kv, s, len);
                ljson_encodeval(L, -1, doc, kv+1);
                unsafe_yyjson_mut_obj_add(val, kv, kv+1, vlen++);
            }
            lua_pop(L, 1);
        }
    }
}

static void ljson_encodeval(lua_State *L, int idx, yyjson_mut_doc *doc, yyjson_mut_val *val) {
    switch (lua_type(L, idx)) {
    case LUA_TNIL:
        unsafe_yyjson_set_null(val);
        break;
    case LUA_TBOOLEAN:
        unsafe_yyjson_set_bool(val, lua_toboolean(L, idx));
        break;
    case LUA_TNUMBER:
        if (lua_isinteger(L, idx))
            unsafe_yyjson_set_sint(val, lua_tointeger(L, idx));
        else
            unsafe_yyjson_set_real(val, lua_tonumber(L, idx));
        break;
    case LUA_TSTRING:
        {
            size_t len;
            const char *s = lua_tolstring(L, idx, &len);
            unsafe_yyjson_set_strn(val, s, len);
        }
        break;
    default:
        ljson_encodeobj(L, idx, doc, val);
    }
}

static int ljson_encoder(lua_State *L) {
    yyjson_mut_doc *doc = (yyjson_mut_doc*)lua_touserdata(L, 2);
    yyjson_write_flag flag = (yyjson_write_flag)lua_tointeger(L, 3);
    char **result = (char**)lua_touserdata(L, 4);
    yyjson_write_err err;
    size_t len;
    yyjson_mut_val *root = unsafe_yyjson_mut_val(doc, 1);
    ljson_encodeval(L, 1, doc, root);
    yyjson_mut_doc_set_root(doc, root);
    *result = yyjson_mut_write_opts(doc, flag, NULL, &len, &err);
    if (*result == NULL) {
        luaL_pushfail(L);
        lua_pushstring(L, err.msg);
        lua_pushinteger(L, err.code);
        return 3;
    }
    return lua_pushlstring(L, *result, len), 1;
}

static int Ljson_encode(lua_State *L) {
    yyjson_write_flag flag = ljson_checkwriteflags(L, 2);
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    char *result = NULL;
    int r;
    if (doc == NULL) return luaL_error(L, "out of memory");
    lua_settop(L, 1);
    lua_pushcfunction(L, ljson_encoder);
    lua_insert(L, 1);
    lua_pushlightuserdata(L, doc);
    lua_pushinteger(L, flag);
    lua_pushlightuserdata(L, &result);
    r = lua_pcall(L, 4, LUA_MULTRET, 0);
    yyjson_mut_doc_free(doc);
    free(result);
    if (r != LUA_OK) {
        luaL_pushfail(L);
        lua_insert(L, -2);
        return 2;
    }
    return lua_gettop(L);
}

static int Ljson_array(lua_State *L) {
    if (lua_getmetatable(L, 1)) {
        lua_pushliteral(L, LYYJSON_TYPE_ARR);
        lua_setfield(L, -2, LYYJSON_TYPE_FIELD);
        return (lua_settop(L, 1), 1);
    }
    return (lua_settop(L, 1), luaL_setmetatable(L, LYYJSON_TYPE_ARR), 1);
}

static int Ljson_object(lua_State *L) {
    if (lua_getmetatable(L, 1)) {
        lua_pushliteral(L, LYYJSON_TYPE_OBJ);
        lua_setfield(L, -2, LYYJSON_TYPE_FIELD);
        return (lua_settop(L, 1), 1);
    }
    return (lua_settop(L, 1), luaL_setmetatable(L, LYYJSON_TYPE_OBJ), 1);
}

LUALIB_API int luaopen_json(lua_State *L) {
    luaL_Reg libs[] = {
#define ENTRY(name) { #name, Ljson_##name }
        ENTRY(encode),
        ENTRY(decode),
        ENTRY(array),
        ENTRY(object),
#undef  ENTRY
        { "version", NULL },
        { NULL, NULL }
    };
    luaL_newlib(L, libs);
    lua_pushliteral(L, YYJSON_VERSION_STRING);
    lua_setfield(L, -2, "version");
    if (luaL_newmetatable(L, LYYJSON_ARR_TYPE)) {
        lua_pushliteral(L, LYYJSON_TYPE_ARR);
        lua_setfield(L, -2, LYYJSON_TYPE_FIELD);
    }
    if (luaL_newmetatable(L, LYYJSON_OBJ_TYPE)) {
        lua_pushliteral(L, LYYJSON_TYPE_OBJ);
        lua_setfield(L, -2, LYYJSON_TYPE_FIELD);
    }
    lua_pop(L, 2);
    return 1;
}

/* cc: flags+='-s -O2 -mdll -DLUA_BUILD_AS_DLL'
 * cc: libs+='-llua54' output='json.dll' */

