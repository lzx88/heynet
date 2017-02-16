#include <string.h>
#include <stdlib.h>
#include "msvcint.h"

#include "lua.h"
#include "lauxlib.h"
#include "zproto.h"

#define ENCODE_BUFFERSIZE (2 << 10)
#define ENCODE_MAXSIZE (16 << 20)
#define ENCODE_BUFDEC 5//编码缓存区调整速度
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
#define for_luatable(L)  for(lua_pushnil(L); 0 != lua_next(L, -2); lua_pop(L, 1))
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
			tp->response = -1;
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
			ty->n = 0;
			ty->f = NULL;
			if_getluatable(L, "field"){
				ty->n = zlua_getsize(L);
				ty->f = pool_alloc(Z->mem, sizeof(*ty->f) * ty->n);
				int n = 0;
				ty->maxn = ty->n;//最长feild 长度 ty->n + 参差数量
				int lasttag = 0;
				for_luatable(L){
					assert(n < ty->fn);
					struct field *tf = ty->f[n++];
					tf->name = __zgetfieldstring(L, Z, "name");
					tf->key = ZK_NULL;
					zlua_getfdata(L, "tag", &tf->tag);
					zlua_getfdata(L, "key", &tf->key);
					zlua_getfdata(L, "type", &tf->type);
					if (tf->tag != (lasttag + 1))
						++ty->maxn;
					lasttag = tf->tag;
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
		struct zproto_type* request = zproto_import(zp, p->request);
		assert(request);
		lua_pushstring(L, request->name);
		lua_pushinteger(L, p->tag);
		lua_pushlightuserdata(L, request);
		struct zproto_type* response = zproto_import(zp, p->response);
		if (response)
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
		if (++halftimes >= ENCODE_BUFDEC) {
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

struct encode_ud {
	lua_State *L;
	struct zproto_type *zt;
	int tbl_index;
	int deep;
};

static int
encode(const struct zproto_encode_arg *args) {
	struct encode_ud *self = args->ud;
	const struct field &f = *args->pf;
	lua_State *L = self->L;

	if (self->deep >= ENCODE_DEEPLEVEL)
		return luaL_error(L, "The table is too deep");
	if (f.key != ZK_NULL) {
		if (args->index == 0){
			lua_getfield(L, self->tbl_index, f.name);
			if (lua_isnil(L, -1)) {
				lua_pop(L, 1);
				return ZPROTO_CB_NIL;
			}
			if (!lua_istable(L, -1)) {
				lua_pop(L, 1);
				return luaL_error(L, ".*%s(%d) should be a table (Is a %s)", f.name, f.tag, lua_typename(L, lua_type(L, -1)));
			}
		}
		if (f.key == ZK_ARRAY)
			lua_geti(L, -1, args->index + 1);
		else if (f.key == ZK_MAP) {
			lua_pushnil(L);
			if (!lua_next(L, -2)) {
				// iterate end
				lua_pop(L, 1);
				return ZPROTO_CB_NIL;
			}
			if (lua_isinteger(L, -2)) {
				char tmp[8] = { 0 };
				sz = sprintf(tmp, "%d", lua_tointeger(L, -2));
				if (sz > args->length)
					return ZPROTO_CB_ERROR;
				memcpy(args->value, tmp, sz);
			}
			else(lua_isstring()) {
				size_t sz = 0;
				const char *str = lua_tolstring(L, -2, &sz);
				if (sz > args->length)
					return ZPROTO_CB_ERROR;
				memcpy(args->value, str, sz);
			}
			else
				return luaL_error(L, ".*%s(%d) should be a map key type (Is a %s)", f.name, f.tag, lua_typename(L, lua_type(L, -1)));
			lua_remove(L, -2);
		}
		else
			assert(false);
		if (lua_isnil(L, -1)) {
			lua_pop(L, 2);
			return ZPROTO_CB_NIL;
		}
	}
	else
		lua_getfield(L, self->tbl_index, f.name);
	if (lua_isnil(L, -1)) {
		lua_pop(L, 1);
		return ZPROTO_CB_NIL;
	}
	switch (f.type) {
	case ZT_INTEGER:
		int isnum;
		lua_Integer v = lua_tointegerx(L, -1, &isnum);
		if (!isnum)
			return luaL_error(L, ".%s[%d] is not a number (Is a %s)", f.name, f.tag, lua_typename(L, lua_type(L, -1)));
		lua_pop(L, 1);
		// notice: in lua 5.2, lua_Integer maybe 52bit
		*(integer *)args->value = v;
		return (v & ~0x7FffFFff) > 0 ? 8 : 4;
	case ZT_BOOL:
		if (!lua_isboolean(L, -1))
			return luaL_error(L, ".%s[%d] is not a bool (Is a %s)", f.name, f.tag, lua_typename(L, lua_type(L, -1)));
		*(integer *)args->value = lua_toboolean(L, -1);
		lua_pop(L, 1);
		return 0;
	case ZT_STRING:
		if (!lua_isstring(L, -1))
			return luaL_error(L, ".%s[%d] is not a string (Is a %s)", f.name, f.tag, lua_typename(L, lua_type(L, -1)));
		size_t sz = 0;
		const char *str = lua_tolstring(L, -1, &sz);
		if (sz > args->length)
			return ZPROTO_CB_ERROR;
		memcpy(args->value, str, sz);
		lua_pop(L, 1);
		return sz;
	default:
		if (f.type < 0)
			return luaL_error(L, "Invalid field type %s.%s", self->st->name, f.name);
		if (!lua_istable(L, -1))
			return luaL_error(L, ".%s[%d] is not a table (Is a %s)", f.name, f.tag, lua_typename(L, lua_type(L, -1)));
		struct encode_ud sub;
		sub.L = L;
		sub.st = zproto_import(ZP, f.type);
		sub.tbl_index = lua_gettop(L);
		sub.deep = self->deep + 1;
		sub.iter = 0;

		lua_pushnil(L);	// prepare an iterator slot
		int sz = zproto_encode(sub.st, args->value, args->length, encode, &sub);
		lua_settop(L, sub.tbl_index - 1);	// pop the value
		return sz;
	}
}

static int
lencode(lua_State *L) {
	void *buffer = lua_touserdata(L, lua_upvalueindex(1));
	int sz = lua_tointeger(L, lua_upvalueindex(2));
	int tbl_index = 3;
	int tag = lua_tointeger(L, 1);
	const struct zproto_type *ty = lua_touserdata(L, 2);
	if (ty == NULL)
		return luaL_argerror(L, 1, "Need a zproto_type object");
	luaL_checktype(L, tbl_index, LUA_TTABLE);
	luaL_checkstack(L, ENCODE_DEEPLEVEL * 2 + 8, NULL);
	
	int rev = 0;
	struct encode_ud self;
	self.L = L;

	for (;;){
		self.zt = ty;
		self.tbl_index = tbl_index;
		self.deep = 0;
		self.iter = 0;

		lua_settop(L, tbl_index);
		lua_pushnil(L);	// for iterator (stack slot 4)
		rev = sz >> 3;//tips: 0pack最坏情况 每16个字节首部会扩充2个字节 encode 头部会预留 size / 8 空间 用于0pack
		buffer += rev;
		int use = zproto_encode(ty, buffer, sz - rev, encode, &self);
		if (use < 0) {
			int newsz = sz << 1;
			buffer = adjust_buffer(L, sz, newsz);
			sz = newsz;
		}
		else {
			adjust_buffer(L, sz, use + rev);
			lua_pushlstring(L, buffer, use);
			return 1;
		}
	}
	return 0;//nohere
}

struct decode_ud {
	lua_State *L;
	const char *array_tag;
	int array_index;
	int result_index;
	int deep;
	int mainindex_tag;
	int key_index;
};

static int
decode(const struct zproto_arg *args) {
	const zproto_field *f = args->pf;
	struct decode_ud *self = args->ud;
	lua_State *L = self->L;
	if (self->deep >= ENCODE_DEEPLEVEL)
		return luaL_error(L, "The table is too deep!");
	if (f->key != ZK_NULL) {

	}
	if (args->idx > 0) {
		// It's array
		if (args->tagname != self->array_tag) {
			self->array_tag = args->tagname;
			lua_newtable(L);
			lua_pushvalue(L, -1);
			lua_setfield(L, self->result_index, args->tagname);
			if (self->array_index) {
				lua_replace(L, self->array_index);
			}
			else {
				self->array_index = lua_gettop(L);
			}
			if (args->idx < 0) {
				// It's a empty array, return now.
				return 0;
			}
		}
	}
	switch (args->type) {
	case ZT_INTEGER:
		// notice: in lua 5.2, 52bit integer support (not 64)
		lua_Integer v = *(integer*)args->val;
		lua_pushinteger(L, v);
		break;
	case ZT_BOOL:
		lua_pushboolean(L, (int*)args->val);
		break;
	case ZT_STRING:
		lua_pushlstring(L, args->val, args->len);
		break;
	default:
		if (args->type < 0)
			return luaL_error(L, "Invalid type");

		struct decode_ud sub;
		int r;
		lua_newtable(L);
		sub.L = L;
		sub.result_index = lua_gettop(L);
		sub.deep = self->deep + 1;
		break;
	}
			sub.array_index = 0;
			sub.array_tag = NULL;
			if (args->mainindex >= 0) {
				// This struct will set into a map, so mark the main index tag.
				sub.mainindex_tag = args->mainindex;
				lua_pushnil(L);
				sub.key_index = lua_gettop(L);

				r = zproto_decode(args->subtype, args->val, args->len, decode, &sub);
				if (r < 0)
					return SPROTO_CB_ERROR;
				if (r != args->len)
					return r;
				lua_pushvalue(L, sub.key_index);
				if (lua_isnil(L, -1)) {
					luaL_error(L, "Can't find main index (tag=%d) in [%s]", args->mainindex, args->tagname);
				}
				lua_pushvalue(L, sub.result_index);
				lua_settable(L, self->array_index);
				lua_settop(L, sub.result_index - 1);
				return 0;
			}
			else {
				sub.mainindex_tag = -1;
				sub.key_index = 0;
				r = sproto_decode(args->subtype, args->val, args->len, decode, &sub);
				if (r < 0)
					return SPROTO_CB_ERROR;
				if (r != args->len)
					return r;
				lua_settop(L, sub.result_index);
				break;
	}
	default:
	}
	if (args->index > 0) {
		lua_seti(L, self->array_index, args->index);
	}
	else {
		if (self->mainindex_tag == args->tagid) {
			// This tag is marked, save the value to key_index
			// assert(self->key_index > 0);
			lua_pushvalue(L, -1);
			lua_replace(L, self->key_index);
		}
		lua_setfield(L, self->result_index, args->tagname);
	}

	return 0;
}
static const void *
getbuffer(lua_State *L, int index, size_t *sz) {
	int t = lua_type(L, index);
	if (t == LUA_TSTRING)
		return lua_tolstring(L, index, sz);
	if (t != LUA_TUSERDATA && t != LUA_TLIGHTUSERDATA)
		luaL_argerror(L, index, "Need a string or userdata");
	*sz = luaL_checkinteger(L, index + 1);
	return lua_touserdata(L, index);
}
static int
ldecode(lua_State *L) {
	const struct zproto_type* ty;
	const void *data;
	struct decode_ud self;
	size_t sz;
	ty = lua_touserdata(L, 1);
	if (!ty)
		return luaL_argerror(L, 1, "Need a zproto_type object");
	data = getbuffer(L, 2, &sz);
	if (!lua_istable(L, -1))
		lua_newtable(L);
	luaL_checkstack(L, ENCODE_DEEPLEVEL * 3 + 8, NULL);
	self.L = L;
	self.result_index = lua_gettop(L);
	self.deep = 0;
	if (0 > zproto_decode(ty, data, sz, decode, &self));
		return luaL_error(L, "decode error");
	lua_settop(L, self.result_index);
	return 1;
}

static int
lpack(lua_State *L) {
	return 1;
}

static int
lunpack(lua_State *L) {
	return 1;
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
		{ "decode", ldecode },
		{ NULL, NULL },
	};
	luaL_newlib(L,l);
	pushfunc_withbuffer(L, "encode", lencode);
	pushfunc_withbuffer(L, "pack", lpack);
	pushfunc_withbuffer(L, "unpack", lunpack);
	return 1;
}