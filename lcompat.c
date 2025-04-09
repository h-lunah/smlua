/*
** $Id: lcompat.c $
** Lua 5.1 compatibility bindings for Lua 5.5
** See Copyright Notice in lua.h
*/

#include <math.h>
#include <string.h>
#include <limits.h>

#include "lua.h"
#include "lauxlib.h"
#include "lcompat.h"

/* ==================== Utility Functions ==================== */

static int lcompat_absindex(lua_State *L) {
    lua_Integer idx = luaL_checkinteger(L, 1);
    if (idx < INT_MIN || idx > INT_MAX)
        return luaL_error(L, "index out of integer range");
    if (idx > 0 || idx <= LUA_REGISTRYINDEX) {
        lua_pushinteger(L, idx);
    } else {
        lua_pushinteger(L, lua_gettop(L) + idx + 1);
    }
    return 1;
}

static int lcompat_to_real_index(lua_State *L) {
    lua_Integer idx = luaL_checkinteger(L, 1);
    if (idx < INT_MIN || idx > INT_MAX)
        return luaL_error(L, "index out of integer range");
    
    if (idx == LUA_GLOBALSINDEX) {
        lua_pushglobaltable(L);
        return 1;
    }
    else if (idx == LUA_ENVIRONINDEX) {
        lua_Debug ar;
        if (lua_getstack(L, 0, &ar) && lua_getinfo(L, "f", &ar)) {
            const char *name = lua_getupvalue(L, -1, 1);
            if (name != NULL && strcmp(name, "_ENV") == 0) {
                lua_remove(L, -2);
                return 1;
            }
            lua_pop(L, 2);
        }
        lua_pushglobaltable(L);
        return 1;
    }
    
    lua_pushinteger(L, idx);
    return 1;
}

/* ==================== Math Compatibility ==================== */

static int math_log10(lua_State *L) {
    lua_pushnumber(L, log10(luaL_checknumber(L, 1)));
    return 1;
}

static int math_frexp(lua_State *L) {
    int e;
    lua_pushnumber(L, frexp(luaL_checknumber(L, 1), &e));
    lua_pushinteger(L, e);
    return 2;
}

static int math_ldexp(lua_State *L) {
    lua_Integer exp = luaL_checkinteger(L, 2);
    if (exp < INT_MIN || exp > INT_MAX) {
        return luaL_error(L, "exponent out of integer range");
    }
    lua_pushnumber(L, ldexp(luaL_checknumber(L, 1), (int)exp));
    return 1;
}

/* ==================== Table Compatibility ==================== */

static int table_maxn(lua_State *L) {
    lua_Number max = 0;
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_pushnil(L);
    while (lua_next(L, 1) != 0) {
        lua_pop(L, 1);
        if (lua_type(L, -1) == LUA_TNUMBER) {
            lua_Number v = lua_tonumber(L, -1);
            if (v > max) max = v;
        }
    }
    lua_pushnumber(L, max);
    return 1;
}

static int table_foreach(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    lua_pushnil(L);
    while (lua_next(L, 1) != 0) {
        lua_pushvalue(L, 2);
        lua_pushvalue(L, -3);
        lua_pushvalue(L, -3);
        lua_call(L, 2, 1);
        if (!lua_isnil(L, -1))
            return 1;
        lua_pop(L, 2);
    }
    return 0;
}

static int table_foreachi(lua_State *L) {
    int i;
    lua_Unsigned n = lua_rawlen(L, 1);
    luaL_checktype(L, 1, LUA_TTABLE);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    if (n > INT_MAX)
        return luaL_error(L, "table too large");

    for (i = 1; i <= (int)n; i++) {
        lua_pushvalue(L, 2);
        lua_pushinteger(L, i);
        lua_rawgeti(L, 1, i);
        lua_call(L, 2, 1);
        if (!lua_isnil(L, -1))
            return 1;
        lua_pop(L, 1);
    }
    return 0;
}

static int table_getn(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_pushinteger(L, (lua_Integer)lua_rawlen(L, 1));
    return 1;
}

/* ==================== String Compatibility ==================== */

static int string_gfind(lua_State *L) {
    luaL_checkstring(L, 1);
    luaL_checkstring(L, 2);
    lua_settop(L, 2);
    lua_getglobal(L, "string");
    lua_getfield(L, -1, "gmatch");
    lua_remove(L, -2);
    lua_pushvalue(L, 1);
    lua_pushvalue(L, 2);
    lua_call(L, 2, 1);
    return 1;
}

/* ==================== Global Function Compatibility ==================== */

static int global_unpack(lua_State *L) {
    lua_getglobal(L, "table");
    lua_getfield(L, -1, "unpack");
    lua_remove(L, -2);
    lua_insert(L, 1);
    lua_call(L, lua_gettop(L) - 1, LUA_MULTRET);
    return lua_gettop(L);
}

static int compat_loadstring(lua_State *L) {
    size_t l;
    const char *s = luaL_checklstring(L, 1, &l);
    const char *chunkname = luaL_optstring(L, 2, s);
    
    lua_getglobal(L, "load");
    lua_pushlstring(L, s, l);
    lua_pushstring(L, chunkname);
    lua_call(L, 2, LUA_MULTRET);
    
    return lua_gettop(L) - 2;
}

/* ==================== Module System Compatibility ==================== */

static int compat_module(lua_State *L) {
    const char *modname = luaL_checkstring(L, 1);
    int loaded = 0;
    
    /* Check if module already exists */
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "loaded");
    lua_getfield(L, -1, modname);
    loaded = !lua_isnil(L, -1);
    lua_pop(L, 3);
    
    /* Create or get module table */
    lua_getglobal(L, "_G");
    lua_getfield(L, -1, modname);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, modname);
    }
    
    /* Set module environment */
    if (lua_getmetatable(L, -1) == 0) {
        lua_newtable(L);
        lua_pushvalue(L, -3);
        lua_setfield(L, -2, "__index");
        lua_setmetatable(L, -2);
    } else {
        lua_pop(L, 1);
    }
    
    /* Register module */
    if (!loaded) {
        lua_getglobal(L, "package");
        lua_getfield(L, -1, "loaded");
        lua_pushvalue(L, -3);
        lua_setfield(L, -2, modname);
        lua_pop(L, 2);
    }
    
    /* Set environment for functions */
    if (lua_gettop(L) > 2) {
        int i;
        for (i = 2; i <= lua_gettop(L); i++) {
            if (lua_isfunction(L, i)) {
                const char *name = lua_getupvalue(L, i, 1);
                int nups = (name != NULL);
                if (nups == 1 && lua_isnil(L, -1)) {
                    lua_pop(L, 1);
                    lua_pushvalue(L, -1);
                    lua_setupvalue(L, i, 1);
                } else {
                    lua_pop(L, nups);
                }
            }
        }
    }
    
    return 1;
}

/* ==================== Environment Functions ==================== */

static int compat_setfenv(lua_State *L) {
    luaL_checktype(L, 2, LUA_TTABLE);
    lua_settop(L, 2);
    
    if (lua_isfunction(L, 1)) {
        if (lua_setupvalue(L, 1, 1) == NULL) {
            lua_pushstring(L, "unable to set environment");
            return lua_error(L);
        }
    }
    else if (lua_isnumber(L, 1)) {
        int level = (int)lua_tointeger(L, 1);
        if (level == 0) {
            lua_pushthread(L);
            lua_pushvalue(L, 2);
            lua_rawset(L, LUA_REGISTRYINDEX);
        }
        else {
            lua_Debug ar;
            if (lua_getstack(L, level, &ar) == 0 ||
                lua_getinfo(L, "f", &ar) == 0 ||
                lua_iscfunction(L, -1)) {
                lua_pushstring(L, "invalid level");
                return lua_error(L);
            }
            lua_pushvalue(L, 2);
            lua_setupvalue(L, -2, 1);
            lua_pop(L, 1);
        }
    }
    else {
        lua_pushstring(L, "invalid argument #1");
        return lua_error(L);
    }
    
    lua_pushvalue(L, 1);
    return 1;
}

static int compat_getfenv(lua_State *L) {
    int narg = lua_gettop(L);
    
    if (narg == 0) {
        lua_pushthread(L);
        lua_rawget(L, LUA_REGISTRYINDEX);
        if (lua_isnil(L, -1)) {
            lua_pop(L, 1);
            lua_pushglobaltable(L);
        }
    }
    else if (lua_isfunction(L, 1)) {
        const char *name = lua_getupvalue(L, 1, 1);
        if (name == NULL || strcmp(name, "_ENV") != 0) {
            lua_pop(L, 1);
            lua_pushglobaltable(L);
        }
    }
    else if (lua_isnumber(L, 1)) {
        int level = (int)lua_tointeger(L, 1);
        lua_Debug ar;
        if (level == 0) {
            lua_pushthread(L);
            lua_rawget(L, LUA_REGISTRYINDEX);
            if (lua_isnil(L, -1)) {
                lua_pop(L, 1);
                lua_pushglobaltable(L);
            }
        }
        else if (lua_getstack(L, level, &ar) != 0 &&
                 lua_getinfo(L, "f", &ar) != 0 &&
                 !lua_iscfunction(L, -1)) {
            const char *name = lua_getupvalue(L, -1, 1);
            if (name == NULL || strcmp(name, "_ENV") != 0) {
                lua_pop(L, 2);
                lua_pushglobaltable(L);
            }
            else {
                lua_remove(L, -2);
            }
        }
        else {
            lua_pushglobaltable(L);
        }
    }
    else {
        lua_pushglobaltable(L);
    }
    
    return 1;
}

/* ==================== Module Registration ==================== */

static const luaL_Reg compat_funcs[] = {
    /* Utility functions */
    {"absindex", lcompat_absindex},
    {"to_real_index", lcompat_to_real_index},
    
    /* Math functions */
    {"log10", math_log10},
    {"frexp", math_frexp},
    {"ldexp", math_ldexp},
    
    /* Table functions */
    {"maxn", table_maxn},
    {"foreach", table_foreach},
    {"foreachi", table_foreachi},
    {"getn", table_getn},
    
    /* String functions */
    {"gfind", string_gfind},
    
    {NULL, NULL}
};

LUAMOD_API int luaopen_compat(lua_State *L) {
    /* Create compat table */
    lua_newtable(L);
    
    /* Register all functions */
    luaL_setfuncs(L, compat_funcs, 0);
    
    /* Register global functions */
    lua_pushcfunction(L, global_unpack);
    lua_setglobal(L, "unpack");
    
    lua_pushcfunction(L, compat_module);
    lua_setglobal(L, "module");
    
    lua_pushcfunction(L, compat_setfenv);
    lua_setglobal(L, "setfenv");
    
    lua_pushcfunction(L, compat_getfenv);
    lua_setglobal(L, "getfenv");
    
    lua_pushcfunction(L, compat_loadstring);
    lua_setglobal(L, "loadstring");

    /* Register in package.loaded */
    lua_getglobal(L, "package");
    if (!lua_isnil(L, -1)) {
        lua_getfield(L, -1, "loaded");
        if (!lua_isnil(L, -1)) {
            // Push compat table again
            lua_pushvalue(L, -3);
            lua_setfield(L, -2, "compat");
            lua_pop(L, 1);  // pop loaded
        }
        lua_pop(L, 1);  // pop package
    }
    
    /* Patch standard libraries */
    lua_getglobal(L, "math");
    if (lua_istable(L, -1)) {
        lua_pushcfunction(L, math_log10);
        lua_setfield(L, -2, "log10");
        
        lua_pushcfunction(L, math_frexp);
        lua_setfield(L, -2, "frexp");
        
        lua_pushcfunction(L, math_ldexp);
        lua_setfield(L, -2, "ldexp");
    }
    lua_pop(L, 1);
    
    lua_getglobal(L, "table");
    if (lua_istable(L, -1)) {
        lua_pushcfunction(L, table_maxn);
        lua_setfield(L, -2, "maxn");
        
        lua_pushcfunction(L, table_foreach);
        lua_setfield(L, -2, "foreach");
        
        lua_pushcfunction(L, table_foreachi);
        lua_setfield(L, -2, "foreachi");
        
        lua_pushcfunction(L, table_getn);
        lua_setfield(L, -2, "getn");
    }
    lua_pop(L, 1);
    
    lua_getglobal(L, "string");
    if (lua_istable(L, -1)) {
        lua_pushcfunction(L, string_gfind);
        lua_setfield(L, -2, "gfind");
    }
    lua_pop(L, 1);
    return 1;
}

/* ==================== Additional Utility Functions ==================== */

void lcompat_pushvalue_at_globalsindex(lua_State *L) {
    lua_pushglobaltable(L);
}

void lcompat_pushglobals(lua_State *L) {
    lua_pushglobaltable(L);
}

void lcompat_setglobal(lua_State *L, const char *name) {
    lua_pushglobaltable(L);
    lua_pushvalue(L, -2);
    lua_setfield(L, -2, name);
    lua_pop(L, 2);
}

int lcompat_getglobal(lua_State *L, const char *name) {
    int type;
    lua_pushglobaltable(L);
    lua_getfield(L, -1, name);
    type = lua_type(L, -1);
    lua_remove(L, -2);
    return type;
}
