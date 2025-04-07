/*
 * lcompat.h
 * Header for Lua 5.1 compatibility layer for Lua 5.4
 */

#ifndef LCOMPAT_H
#define LCOMPAT_H

#include "lua.h"
#include "lauxlib.h"

/* Compatibility pseudo-indices for Lua 5.1 */
#define LUA_GLOBALSINDEX (-10002)
#define LUA_ENVIRONINDEX (-10001)

/* Compatibility macros */
#define lua_objlen(L,i)     lua_rawlen(L, (i))
#define lua_strlen(L,i)     lua_rawlen(L, (i))
#define lua_newtable(L)     lua_createtable(L, 0, 0)
#define lua_equal(L,idx1,idx2)  lua_compare(L,(idx1),(idx2),LUA_OPEQ)
#define lua_lessthan(L,idx1,idx2)  lua_compare(L,(idx1),(idx2),LUA_OPLT)

/* Get global table (equivalent to LUA_GLOBALSINDEX access) */
#define lua_pushglobaltable(L) \
        (lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS))


/* Compatibility API for global table access */
void lcompat_pushvalue_at_globalsindex(lua_State *L);
void lcompat_setglobal(lua_State *L, const char *name);
void lcompat_getglobal(lua_State *L, const char *name);
void lcompat_pushglobals(lua_State *L);

/* Main initialization function */
int luaopen_compat(lua_State *L);

#endif /* LCOMPAT_H */
