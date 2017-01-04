#include <string.h>
#include <stdlib.h>
#include "msvcint.h"

#include "lua.h"
#include "lauxlib.h"
#include "zproto.h"

#define MAX_GLOBALZPROTO 16
#define ENCODE_BUFFERSIZE 2050

#define ENCODE_MAXSIZE 0x1000000
#define ENCODE_DEEPLEVEL 64

#ifndef luaL_newlib /* using LuaJIT */
/*
** set functions from list 'l' into table at top - 'nup'; each
** function gets the 'nup' elements at the top as upvalues.
** Returns with only the table at the stack.
*/
LUALIB_API void luaL_setfuncs (lua_State *L, const luaL_Reg *l, int nup) {
#ifdef luaL_checkversion
	luaL_checkversion(L);
#endif
	luaL_checkstack(L, nup, "too many upvalues");
	for (; l->name != NULL; l++) {  /* fill the table with given functions */
		int i;
		for (i = 0; i < nup; i++)  /* copy upvalues to the top */
			lua_pushvalue(L, -nup);
		lua_pushcclosure(L, l->func, nup);  /* closure with those upvalues */
		lua_setfield(L, -(nup + 2), l->name);
	}
	lua_pop(L, nup);  /* remove upvalues */
}

#define luaL_newlibtable(L,l) \
  lua_createtable(L, 0, sizeof(l)/sizeof((l)[0]) - 1)

#define luaL_newlib(L,l)  (luaL_newlibtable(L,l), luaL_setfuncs(L,l,0))
#endif

#if LUA_VERSION_NUM < 503

#if LUA_VERSION_NUM < 502
static lua_Integer lua_tointegerx(lua_State *L, int idx, int *isnum) {
	if (lua_isnumber(L, idx)) {
		if (isnum) *isnum = 1;
		return lua_tointeger(L, idx);
	}
	else {
		if (isnum) *isnum = 0;
		return 0;
	}
}
#endif

// work around , use push & lua_gettable may be better
#define lua_geti lua_rawgeti
#define lua_seti lua_rawseti

#endif

inline void zlua_getdata(lua_State* L, int idx, int *val) { *val = lua_tointeger(L, idx); }
inline void zlua_getdata(lua_State* L, int idx, char **val){ *val = lua_tostring(L, idx); }
inline void zlua_getfield(lua_State* L, int idx, int k) { lua_rawgeti(L, idx, k); }
inline void zlua_getfield(lua_State* L, int idx, const char* k) { lua_rawgetp(L, idx, k); }
inline bool __zlua_tblcheck(){ if (lua_istable(L, -1)) return true; lua_pop(L, 1); return false; }
#define if_getluatable(L, k) zlua_getfield(L, -1, k); for(int i = 0; i-- == 0 && __zlua_tblcheck(); lua_pop(L, 1))
#define for_luatable(L)  for(lua_pushnil(L); 0 != lua_next(L, -2); lua_pop(L, 1), lua_pop(L, 1))
#define zlua_getsize(L) lua_rawlen(L, -1)
#define zlua_getfdata(L, k, val) zlua_getfield(L, -1, k); zlua_getdata(L, -1, val); lua_pop(L, 1)

static char* __zgetfieldstring(lua_State* L, struct zproto *Z, const chat *key){
	char buf[256] = { 0 };
	zlua_getfdata(L, key, &buf);
	char* tmp = pool_alloc(Z->mem, strlen(buf) + 1);
	strcpy(tmp, buf);
	return tmp;
}

static int
lcreate(lua_State *L) {
	int idx = lua_gettop(L);
	assert(idx == 1);
	struct zproto *Z = zproto_alloc();
	if_getluatable(L, "P"){
		Z->pn = zlua_getsize(L);
		Z->p = pool_alloc(Z->mem, sizeof(*Z->p) * Z->pn);
		int n = 0;
		for_luatable(L){
			assert(n < Z->pn);
			protocol* tp = Z->p[n++];
			zlua_getfield(L, "tag", &tp->tag);
			zlua_getfield(L, "request", &tp->request);
			zlua_getfield(L, "response", &tp->response);
		}
	}
	if_getluatable(L, "T"){
		Z->tn = zlua_getsize(L);
		Z->t = pool_alloc(Z->mem, sizeof(*Z->t) * Z->tn);
		for_luatable(L){
			int tag;
			zlua_getfdata(L, "tag", &tag);
			assert(tag <= z->tn);

			struct type *ty = Z->t[tag - 1];
			ty->name = __zgetfieldstring(L, Z, "name");
			ty->fn = 0;
			ty->f = NULL;
			if_getluatable(L, "field"){
				ty->fn = zlua_getsize(L);
				ty->f = pool_alloc(Z->mem, sizeof(*ty->f) * ty->fn);
				int n = 0;
				for_luatable(L){
					assert(n < ty->fn);
					struct field *tf = ty->f[n++];
					tf->name = __zgetfieldstring(L, Z, "name");
					zlua_getfdata(L, "tag", &tf->tag);
					zlua_getfdata(L, "type", &tf->type);
					zlua_getfdata(L, "key", &tf->key);
				}
			}
		}		
	}

	if (zproto_done(Z)){
		lua_pushlightuserdata(L, Z);
		return 1;
	}
	return 0;
}

static int
lrelease(lua_State *L) {
	struct zproto *Z = lua_touserdata(L,1);
	if (Z == NULL) {
		return luaL_argerror(L, 1, "Need a zproto object");
	}
	zproto_free(Z);
	return 0;
}

static int
lquery(lua_State *L) {
	const char *type_name;
	struct zproto *sp = lua_touserdata(L,1);
	struct zproto_type *st;
	if (sp == NULL) {
		return luaL_argerror(L, 1, "Need a zproto object");
	}
	type_name = luaL_checkstring(L,2);
	st = zproto_type(sp, type_name);
	if (st) {
		lua_pushlightuserdata(L, st);
		return 1;
	}
	return 0;
}

static int
ldump(lua_State *L) {
	struct zproto *Z = lua_touserdata(L, 1);
	if (Z == NULL) {
		return luaL_argerror(L, 1, "Need a zproto_type object");
	}
	zproto_dump(Z);
	return 0;
}

static int
lload(lua_State *L) {

	return 0;
}


int
luaopen_zproto_core(lua_State *L) {
#ifdef luaL_checkversion
	luaL_checkversion(L);
#endif
	luaL_Reg l[] = {
		{ "create", lcreate },
		{ "delete", lrelease },
		{ "dump", ldump },
		{ "load", lload },
		{ "query", lquery },
		{ NULL, NULL },
	};
	luaL_newlib(L,l);
	pushfunction_withbuffer(L, "encode", lencode);
	pushfunction_withbuffer(L, "pack", lpack);
	pushfunction_withbuffer(L, "unpack", lunpack);
	return 1;
}


