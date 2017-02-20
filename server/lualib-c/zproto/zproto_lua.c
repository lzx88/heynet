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

static int zgetfieldint(lua_State* L, const char *key) {
	lua_rawgetp(L, -1, key);
	int isnum;
	lua_Integer result = lua_tointegerx(L, -1,& isnum);
	if (isnum == 0)
		luaL_error(L, "Field %s isn't number", key);
	lua_pop(L, 1);
	return result;
}

static char* zgetfieldname(lua_State* L, struct zproto *zp, const char *key){
	char buf[256] = { 0 };
	lua_rawgetp(L, -1, key);
	int len;
	const char *name = lua_tolstring(L, -1, &len);
	if (NULL == name)
		luaL_error(L, "Field %s isn't string", key);
	char* result = pool_alloc(zp->mem, len + 1);
	strcpy(result, name);
	lua_pop(L, 1);
	return result;
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
	lua_rawgetp(L, -1, "P");
	if (lua_istable(L, -1)) {
		Z->pn = lua_rawlen(L, -1);
		Z->p = pool_alloc(Z->mem, sizeof(*Z->p) * Z->pn);
		int n = 0;
		for (lua_pushnil(L); 0 != lua_next(L, -2); lua_pop(L, 1)) {
			assert(n < Z->pn);
			protocol* tp = Z->p[n++];
			zgetfieldint(L, "tag", &tp->tag);
			zgetfieldint(L, "request", &tp->request);
			tp->response = -1;
			zgetfieldint(L, "response", &tp->response);
		}
	}
	lua_pop(L, 1);
	for (int _ = 0; _-- == 0 && (lua_istable(L, -1) ? lua_pop(L, 1) :\
		false) : ); lua_pop(L, 1))
	if_getluatable(L, "P"){
		
	}
	if_getluatable(L, "T"){
		Z->tn = lua_rawlen(L, -1);
		Z->t = pool_alloc(Z->mem, sizeof(*Z->t) * Z->tn);
		for_luatable(L){
			int tag = zgetfieldint(L, "tag");
			assert(tag <= z->tn);

			struct type *ty = Z->t[tag];
			ty->name = zgetfieldname(L, Z, "name");
			ty->n = 0;
			ty->f = NULL;
			if_getluatable(L, "field"){
				ty->n = lua_rawlen(L, -1);
				ty->f = pool_alloc(Z->mem, sizeof(*ty->f) * ty->n);
				int n = 0;
				ty->maxn = ty->n;//最长feild 长度 ty->n + 参差数量
				int lasttag = 0;
				for_luatable(L){
					assert(n < ty->fn);
					struct field *tf = ty->f[n++];
					tf->name = zgetfieldname(L, Z, "name");
					tf->tag = zgetfieldint(L, "tag");
					tf->key = zgetfieldint(L, "key");
					tf->type = zgetfieldint(L, "type");
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
	int table_index;
	int deep;
};

static int
encode(const struct zproto_arg *args) {
	struct encode_ud *self = args->ud;
	const struct field &f = *args->pf;
	lua_State *L = self->L;
	int sz = 0;
	if (self->deep >= ENCODE_DEEPLEVEL)
		return luaL_error(L, "The table is too deep");
	if (f.key == ZK_NULL) 
		lua_getfield(L, self->table_index, f.name);
	else {
		if ((f.key == ZK_ARRAY && args->index == 1) ||
			(f.key == ZK_MAP && args->index == -1)) {
			lua_getfield(L, self->table_index, f.name);
			if (lua_isnil(L, -1)) {
				lua_pop(L, 1);
				return ZPROTO_CB_NIL;
			}
			if (!lua_istable(L, -1))
				return luaL_error(L, ".*%s(%d) should be a table (Is a %s)", f.name, f.tag, lua_typename(L, lua_type(L, -1)));
		}
		if (f.key == ZK_ARRAY)
			lua_geti(L, -1, args->index);
		else if (f.key == ZK_MAP && args->index < 0) {
			lua_pushnil(L);
			if (!lua_next(L, -2)) {
				// iterate end
				lua_pop(L, 1);
				return ZPROTO_CB_NIL;
			}
			if (lua_isinteger(L, -2)) {
				char tmp[8] = { 0 };
				sz = 1 + sprintf(tmp, "%d", lua_tointeger(L, -2));
				if (sz > args->length)
					return ZPROTO_CB_MEM;
				strcpy(args->value, tmp);
			}
			else(lua_isstring(L, -2)) {
				const char *str = lua_tolstring(L, -2, &sz);
				++sz;
				if (sz > args->length)
					return ZPROTO_CB_MEM;
				strcpy(args->value, str);
			}
			else
				return luaL_error(L, ".*%s(%d) should be a map key type (Is a %s)", f.name, f.tag, lua_typename(L, lua_type(L, -1)));
			lua_remove(L, -2);
			return sz;
		}
		if (lua_isnil(L, -1)) {
			lua_pop(L, 2);
			return ZPROTO_CB_NIL;
		}
	}
	if (lua_isnil(L, -1)) {
		lua_pop(L, 1);
		return ZPROTO_CB_NIL;
	}
	switch (f.type) {
	case ZT_INTEGER:// notice: in lua 5.2, lua_Integer maybe 52bit
		int isnum;
		lua_Integer v = lua_tointegerx(L, -1, &isnum);
		if (!isnum)
			return luaL_error(L, ".%s[%d] is not a number (Is a %s)", f.name, f.tag, lua_typename(L, lua_type(L, -1)));
		lua_pop(L, 1);
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
		const char *str = lua_tolstring(L, -1, &sz);
		if (sz > args->length)
			return ZPROTO_CB_MEM;
		memcpy(args->value, str, sz);
		lua_pop(L, 1);
		return sz;
	default:
		if (!lua_istable(L, -1))
			return luaL_error(L, ".%s[%d] is not a table (Is a %s)", f.name, f.tag, lua_typename(L, lua_type(L, -1)));
		struct encode_ud sub;
		sub.L = L;
		sub.table_index = lua_gettop(L);
		sub.deep = self->deep + 1;
		sub.iter = 0;
		sub.st = zproto_import(ZP, f.type);
		if (sub.st == NULL)
			return luaL_error(L, "Invalid field type %s.%s", self->st->name, f.name);
		lua_pushnil(L);	// prepare an iterator slot
		int sz = zproto_encode(sub.st, args->value, args->length, encode, &sub);
		lua_settop(L, sub.table_index - 1);	// pop the value
		return sz;
	}
}

static int
lencode(lua_State *L) {
	struct encode_ud self;
	int rev;
	void *buffer = lua_touserdata(L, lua_upvalueindex(1));
	int sz = lua_tointeger(L, lua_upvalueindex(2));
	int tag = lua_tointeger(L, 1);
	const struct zproto_type *ty = lua_touserdata(L, 2);
	if (ty == NULL)
		return luaL_argerror(L, 1, "Need a zproto_type object");
	self.L = L;
	luaL_checktype(L, self.table_index, LUA_TTABLE);
	luaL_checkstack(L, ENCODE_DEEPLEVEL * 2 + 8, NULL);
	
	for (;;){
		self.table_index = 3;
		self.deep = 0;
		self.iter = 0;

		lua_settop(L, self.table_index);
		lua_pushnil(L);	// for iterator (stack slot 4)
		int rev = sz >> 3;//tips: 0pack最坏情况 每16个字节首部会扩充2个字节 encode 头部会预留 size / 8 空间 用于0pack
		buffer += rev;
		int use = zproto_encode(ty, buffer, sz - rev, encode, &self);
		if (use == -1) {
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
	int array_index;
	int result_index;
	int deep;
	const char *key;
};

static int
decode(const struct zproto_arg *args) {
	const zproto_field *f = args->pf;
	struct decode_ud *self = args->ud;
	lua_State *L = self->L;
	if (self->deep >= ENCODE_DEEPLEVEL)
		return luaL_error(L, "The table is too deep!");
	if ((f->key == ZK_ARRAY && args->index == 1) ||
		(f->key == ZK_MAP && args->index == -1)) {
		lua_newtable(L);
		lua_pushvalue(L, -1);
		lua_setfield(L, self->result_index, f->name);
		lua_replace(L, self->array_index);
	}
	if (f->key == ZK_MAP && args->index < 0){
		self->key = args->value;
		return 0;
	}
	switch (f->type) {
	case ZT_INTEGER:// notice: in lua 5.2, 52bit integer support (not 64)
		lua_pushinteger(L, *(integer*)args->value);
		break;
	case ZT_BOOL:
		lua_pushboolean(L, (int*)args->value);
		break;
	case ZT_STRING:
		lua_pushlstring(L, args->value, args->length);
		break;
	default:
		if (f->type < 0)
			return luaL_error(L, "Invalid type");
		struct decode_ud sub;
		lua_newtable(L);
		sub.L = L;
		sub.deep = self->deep + 1;
		sub.result_index = lua_gettop(L);
		sub.key = NULL;
		sub.array_index = self.result_index + 1;
		lua_pushnil(L);
		int sz = zproto_decode(zproto_import(ZP, f->type), args->value, args->length, args->shift, decode, &sub);
		if (sz < 0)
			return luaL_error(L, "Decode error!");
		if (sz != args->length)
			return sz;
		lua_settop(L, sub.result_index);
		break;
	}
	if (args->index == 0)
		lua_setfield(L, self->result_index, args->tagname);
	else if (args->index > 0)	{
		if (ZK_ARRAY == f->key)
			lua_seti(L, self->array_index, args->index);
		else if (ZK_MAP == f->key)
			lua_setfield(L, self->result_index, self->key);
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
	self.key = NULL;
	self.array_index = self.result_index + 1;
	lua_pushnil(L);
	if (zproto_decode(ty, data, sz, decode, &self) < 0);
		return luaL_error(L, "Decode error!");
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