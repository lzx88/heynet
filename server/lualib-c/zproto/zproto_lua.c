#include <string.h>
#include <stdlib.h>
#include "msvcint.h"

#include "lua.h"
#include "lauxlib.h"
#include "zproto.h"

#define ENCODE_BUFFERSIZE 2 << 10 //0pack最坏情况 每16个字节首部会扩充2个字节 encode 头部会预留 size / 8 空间 用于0pack
#define ENCODE_MAXSIZE 16 << 20
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
	int b = lua_isnumber(L, idx);
	if (isnum) *isnum = b ? 1 : 0;
	return b ? lua_tointeger(L, idx) : 0;
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
#define if_getluatable(L, k) zlua_getfield(L, -1, k); for(int i = 0; i-- == 0 && lua_istable(L, -1) || (lua_pop(L, 1); false); lua_pop(L, 1))
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

/* global zproto pointer for multi states
NOTICE : It is not thread safe
*/
static struct zproto *ZP = NULL;

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

			struct type *ty = Z->t[tag];
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
					tf->key = ZK_NULL;
					zlua_getfdata(L, "tag", &tf->tag);
					zlua_getfdata(L, "key", &tf->key);
					zlua_getfdata(L, "type", &tf->type);
				}
			}
		}		
	}

	if (zproto_done(Z)){
		lua_pushlightuserdata(L, Z);
		return 1;
	}
	zproto_free(Z);
	return 0;
}

static int
ldelete(lua_State *L) {
	struct zproto *z = lua_touserdata(L,1);
	if (z == NULL) {
		return luaL_argerror(L, 1, "Need a zproto object");
	}
	zproto_free(z);
	return 0;
}

static int
lsave(lua_State *L) {
	assert(!ZP);
	ZP = lua_touserdata(L, 1);
	return 0;
}

static int
lload(lua_State *L) {
	struct zproto* zp = *ZP;
	assert(zp);
	for (int i = 0; i < zp->pn; ++i)
	{
		struct protocol* p = zp->p[i];
		lua_pushinteger(L, p->tag);
		struct zproto_type* request = zproto_import(zp, p->request);
		assert(request);
		lua_pushstring(L, request->name);
		lua_pushlightuserdata(L, request);
		struct zproto_type* response = zproto_import(zp, p->response);
		if (!response)
			lua_pushlightuserdata(L, response);
		else
			lua_pushnil(L);
	}
	return 4 * zp->pn;
}

static int
ldump(lua_State *L) {
	struct zproto *z = lua_touserdata(L, 1);
	if (z == NULL) {
		return luaL_argerror(L, 1, "Need a zproto_type object");
	}
	zproto_dump(z);
	return 0;
}

static void
pushfunc_withbuffer(lua_State *L, const char * name, lua_CFunction func) {
	lua_newuserdata(L, ENCODE_BUFFERSIZE);
	lua_pushinteger(L, ENCODE_BUFFERSIZE);
	lua_pushinteger(L, 0);
	lua_pushcclosure(L, func, 3);
	lua_setfield(L, -2, name);
}

static void *
adjust_buffer(lua_State *L, int oldsize, int newsize) {
	int halftimes = lua_tointeger(L, lua_upvalueindex(3));
	if (newsize <= (oldsize >> 1)) {
		if (++halftimes >= 5) {
			halftimes = 0;
			oldsize >>= 1;
		}
	}
	else if (newsize <= oldsize)
		return NULL;//no here
	else {
		halftimes = 0;
		while (newsize > oldsize)
			oldsize <<= 1;
	}
	if (oldsize > ENCODE_MAXSIZE) {
		luaL_error(L, "object is too large (>%d)", ENCODE_MAXSIZE);
		return NULL;
	}

	void *buf = NULL;
	lua_pushinteger(L, halftimes);
	lua_replace(L, lua_upvalueindex(3));
	if (halftimes == 0)	{
		buf = lua_newuserdata(L, oldsize);
		lua_replace(L, lua_upvalueindex(1));
		lua_pushinteger(L, oldsize);
		lua_replace(L, lua_upvalueindex(2));
	}
	else
		buf = lua_touserdata(L, lua_upvalueindex(1));
	return buf;
}

int
luaopen_zproto_core(lua_State *L) {
#ifdef luaL_checkversion
	luaL_checkversion(L);
#endif
	luaL_Reg l[] = {
		{ "create", lcreate },
		{ "delete", ldelete },
		{ "save", lsave },
		{ "load", lload },
		{ "dump", ldump },
		{ NULL, NULL },
	};
	luaL_newlib(L,l);
	pushfunc_withbuffer(L, "encode", lencode);
	pushfunc_withbuffer(L, "pack", lpack);
	pushfunc_withbuffer(L, "unpack", lunpack);
	return 1;
}


