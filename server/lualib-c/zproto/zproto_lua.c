#include <string.h>
#include <stdlib.h>
#include "msvcint.h"

#include "lua.h"
#include "lauxlib.h"
#include "zproto.h"

#define ENCODE_MINSIZE (2 << 10)
#define ENCODE_MAXSIZE (16 << 20)
#define ENCODE_BUFDEC 5//编码缓冲区 所使用长度比总长小2倍 达到 5 次就缩小一倍
#define ENCODE_DEEPLEVEL 64

#ifndef luaL_newlib /* using LuaJIT */
/*
** set functions from list 'l' into table at top - 'nup'; each
** function gets the 'nup' elements at the top as upvalues.
** Returns with only the table at the stack.
*/
LUALIB_API void luaL_setfuncs (lua_State* L, const luaL_Reg* l, int nup) {
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
static lua_Integer lua_tointegerx(lua_State* L, int idx, int* isnum) {
	int b = lua_isnumber(L, idx);
	if (isnum) *isnum = b ? 1 : 0;
	return b ? lua_tointeger(L, idx) : 0;
}

#endif

// work around , use push & lua_gettable may be better
#define lua_geti lua_rawgeti
#define lua_seti lua_rawseti

#endif

static lua_Integer zgetfieldint(lua_State* L, const char* key) {
	lua_getfield(L, -1, key);
	int isnum;
	lua_Integer result = lua_tointegerx(L, -1, &isnum);
	if (isnum == 0)
		luaL_error(L, "Field %s isn't number", key);
	lua_pop(L, 1);
	return result;
}

static char* zgetfieldstring(lua_State* L, struct zproto* zp, const char* key) {
	lua_getfield(L, -1, key);
	size_t len;
	const char* name = lua_tolstring(L, -1, &len);
	if (NULL == name)
		luaL_error(L, "Field %s isn't string", key);
	char* result = zproto_alloc(zp, len + 1);
	strcpy(result, name);
	lua_pop(L, 1);
	return result;
}

/* global zproto pointer for multi states
NOTICE : It is not thread safe
*/
static struct zproto* ZP = NULL;

static int
lcreate(lua_State* L) {
	int n;
	struct zproto zp;
	zproto_init(&zp);
	luaL_checktype(L, 1, LUA_TTABLE);
	lua_getfield(L, -1, "P");
	if (lua_istable(L, -1)) {
		zp.pn = lua_rawlen(L, -1);
		zp.p = (struct zprotocol*)zproto_alloc(&zp, sizeof(*zp.p) * zp.pn);
		for (n = 0; n < zp.pn; ++n){
			lua_geti(L, -1, n + 1);
			if (lua_istable(L, -1)) {
				zp.p[n].tag = zgetfieldint(L, "tag");
				zp.p[n].request = zgetfieldint(L, "request");
				zp.p[n].response = zgetfieldint(L, "response");
			}
			lua_pop(L, 1);
		}
	}
	lua_pop(L, 1);

	lua_getfield(L, -1, "T");
	if (lua_istable(L, -1)) {
		zp.tn = 1 + lua_rawlen(L, -1);
		zp.t = (struct ztype*)zproto_alloc(&zp, sizeof(*zp.t) * zp.tn);
		for (n = 0; n < zp.tn; ++n) {
			struct ztype* ty = &zp.t[n];
			lua_geti(L, -1, n);
			ty->name = zgetfieldstring(L, &zp, "name");
			ty->n = 0;
			ty->maxn = 0;
			ty->f = NULL;
			lua_getfield(L, -1, "field");
			int lasttag = 0;
			if (lua_istable(L, -1)) {
				ty->n = lua_rawlen(L, -1);
				ty->maxn = ty->n;
				ty->f = (struct zfield*)zproto_alloc(&zp, sizeof(*ty->f) * ty->n);
				for (int j = 0; j < ty->n ; ++j) {
					lua_geti(L, -1, j + 1);
					if (lua_istable(L, -1)) {
						ty->f[j].name = zgetfieldstring(L, &zp, "name");
						ty->f[j].tag = zgetfieldint(L, "tag");
						ty->f[j].key = zgetfieldint(L, "key");
						ty->f[j].type = zgetfieldint(L, "type");
						if (ty->f[j].tag != (lasttag + 1))
							++ty->maxn;
						lasttag = ty->f[j].tag;
					}
					lua_pop(L, 1);
				}
			}
			lua_pop(L, 2);
		}
	}
	lua_pop(L, 1);
	struct zproto* result = zproto_done(&zp);
	lua_pushlightuserdata(L, result);
	return 1;
}

static int
ldelete(lua_State* L) {
	struct zproto* z = lua_touserdata(L,1);
	if (z == NULL) {
		return luaL_argerror(L, 1, "Need a zproto object.");
	}
	zproto_free(z);
	return 0;
}

static int
lsave(lua_State* L) {
	luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
	if(ZP)
		return luaL_argerror(L, 1, "Need free zproto before use.");
	ZP = lua_touserdata(L, 1);
	return 0;
}

static int
lload(lua_State* L) {
	struct zproto* zp = ZP;
	const struct zprotocol* p;
	size_t t = lua_type(L, 1);
	if (t == LUA_TNUMBER)
		p = zproto_findtag(zp, lua_tointeger(L, 1));
	else if (t == LUA_TSTRING)
		p = zproto_findname(zp, luaL_tolstring(L, 1, &t));
	else
		return luaL_argerror(L, 1, "Need a string or integer.");
	if (p == NULL)
		return luaL_error(L, "Protocol not exist!");
	lua_pushinteger(L, p->tag);
	const struct ztype* request = zproto_import(zp, p->request);
	lua_pushstring(L, request->name);
	lua_pushlightuserdata(L, (void*)request);
	const struct ztype* response = zproto_import(zp, p->response);
	if (response)
		lua_pushlightuserdata(L, (void*)response);
	else
		lua_pushnil(L);
	return 4;
}

static int
ldump(lua_State* L) {
	struct zproto* z = lua_touserdata(L, 1);
	if (z == NULL) {
		return luaL_argerror(L, 1, "Need a zproto_type object");
	}
	//zproto_dump(z);
	
	
	return 0;
}

struct encode_ud {
	lua_State* L;
	const char* tyname;
	int table_index;
	int iter_index;
	int deep;
};
static void* 
double_buffer(lua_State* L, int* size) {
	*size *= 2;
	if (*size > ENCODE_MAXSIZE) {
		luaL_error(L, "object is too large (>%d)", ENCODE_MAXSIZE);
		return NULL;
	}
	char* buf = lua_newuserdata(L, *size);
	lua_replace(L, lua_upvalueindex(1));
	lua_pushinteger(L, *size);
	lua_replace(L, lua_upvalueindex(2));
	lua_pushinteger(L, 0);
	lua_replace(L, lua_upvalueindex(3));
	return buf;
}
static void* 
adjust_buffer(lua_State* L, int* size, int sz) {
	char* buffer = NULL;
	if (*size > ENCODE_MINSIZE && (sz << 1) < *size) {
		int halftimes = lua_tointeger(L, lua_upvalueindex(3));
		if (++halftimes >= ENCODE_BUFDEC) {
			halftimes = 0;
			(*size) >>= 1;
			buffer = lua_newuserdata(L, *size);
			lua_replace(L, lua_upvalueindex(1));
			lua_pushinteger(L, *size);
			lua_replace(L, lua_upvalueindex(2));
		}
		lua_pushinteger(L, halftimes);
		lua_replace(L, lua_upvalueindex(3));
	}
	return buffer;
}
static int
encode(struct zproto_arg* args) {
	struct encode_ud* self = args->ud;
	const struct zfield* f = args->pf;
	lua_State* L = self->L;
	int sz = 0;
	if (self->deep >= ENCODE_DEEPLEVEL)
		return luaL_error(L, "The table is too deep");
	if (f->key == ZK_NULL) 
		lua_getfield(L, self->table_index, f->name);
	else {
		if ((f->key == ZK_ARRAY && args->index == 1) ||
			(f->key == ZK_MAP && args->index == -1)) {
			lua_getfield(L, self->table_index, f->name);
			if (lua_isnil(L, -1)) {
				lua_pop(L, 1);
				return ZCB_NIL;
			}
			if (!lua_istable(L, -1))
				return luaL_error(L, "%s.%s should be a table (Is a %s)", self->tyname, f->name, lua_typename(L, lua_type(L, -1)));
		}
		if (f->key == ZK_ARRAY)
			lua_geti(L, -1, args->index);
		else if (f->key == ZK_MAP && args->index < 0) {
			if (self->iter_index == 0) {
				lua_pushnil(L);
				self->iter_index = lua_gettop(L);
			}
			if (!lua_next(L, -2)) {
				lua_pop(L, 1);
				self->iter_index = 0;
				return ZCB_NIL;// iterate end
			}
			if (LUA_TNUMBER == lua_type(L, -2)) {
				if (16 > args->length)
					return ZCB_MEM;
				sz = sprintf(args->value, "\x20%d%s", (int)lua_tointeger(L, -2), "\0") + 1;
			}
			else {
				const char* str = lua_tolstring(L, -2, (size_t*)&sz);
				if (NULL == str)
					return luaL_error(L, "%s.%s should be a map key type (Is a %s)", self->tyname, f->name, lua_typename(L, lua_type(L, -1)));
				if (++sz > args->length)
					return ZCB_MEM;
				strcpy(args->value, str);
			}
			return sz;
		}
		if (lua_isnil(L, -1)) {
			lua_pop(L, 2);
			return ZCB_NIL;
		}
	}
	if (lua_isnil(L, -1)) {
		lua_pop(L, 1);
		return luaL_error(L, "%s.%s is nil.", self->tyname, f->name);
	}
	switch (f->type) {
	case ZT_INTEGER: // notice: in lua 5.2, lua_Integer maybe 52bit
		if (LUA_TNUMBER != lua_type(L, -1))
			return luaL_error(L, "%s.%s is not a number (Is a %s)", self->tyname, f->name, lua_typename(L, lua_type(L, -1)));
		integer v = (integer)lua_tonumber(L, -1);
		*(integer* )args->value = v;
		lua_pop(L, 1);
		sz = v & ~0x7FffFFff ? 8 : 4;
		break;
	case ZT_BOOL:
		if (!lua_isboolean(L, -1))
			return luaL_error(L, "%s.%s is not a bool (Is a %s)", self->tyname, f->name, lua_typename(L, lua_type(L, -1)));
		args->value = (void*)lua_toboolean(L, -1);
		lua_pop(L, 1);
		sz = 0;
		break;
	case ZT_STRING:
		if (!lua_isstring(L, -1))
			return luaL_error(L, "%s.%s is not a string (Is a %s)", self->tyname, f->name, lua_typename(L, lua_type(L, -1)));
		const char* str = lua_tolstring(L, -1, (size_t*)&sz);
		if (sz > args->length)
			return ZCB_MEM;
		memcpy(args->value, str, sz);
		lua_pop(L, 1);
		break;
	default:
		if (!lua_istable(L, -1))
			return luaL_error(L, "%s.%s is not a table (Is a %s)", self->tyname, f->name, lua_typename(L, lua_type(L, -1)));
		const struct ztype* ty = zproto_import(ZP, f->type);
		if (NULL == ty)
			return luaL_error(L, "encode error!");
		struct encode_ud sub;
		sub.tyname = ty->name;
		sub.L = L;
		sub.table_index = lua_gettop(L);
		sub.iter_index = 0;
		sub.deep = self->deep + 1;
		sz = zproto_encode(ty, args->value, args->length, encode, &sub);
		lua_settop(L, sub.table_index - 1);	// pop the value
		break;
	}
	return sz;
}

static int
encode_header(lua_State* L, char* buffer, int size) {
	if (size < 8)
		return ZCB_MEM;
	*buffer = ZP->endian;
	*(buffer + 1) = 1;
	*(uint16*)(buffer + 2) = luaL_checkinteger(L, 3);//tag
	if (lua_type(L, 4) != LUA_TNUMBER)
		return 4;
	*(buffer + 1) = 0;
	*(uint32*)(buffer + 4) = lua_tointeger(L, 4);//session
	return 8;
}

static int
lencode(lua_State* L) {
	struct encode_ud self;
	self.L = L;
	char* buffer = lua_touserdata(L, lua_upvalueindex(1));
	int size = lua_tointeger(L, lua_upvalueindex(2));

	int rev = size >> 3;//tips: 0pack最坏情况每16个字节头部会增加2个字节，预留1/8给0pack
	char* header = buffer + rev;
	const int hlen = encode_header(L, header, size - rev);
	if (hlen < 0)
		return luaL_error(L, "Encode err no.%d!", hlen);
	int sz = hlen + rev;

	const struct ztype* ty = lua_touserdata(L, 1);
	if (ty == NULL)
		return luaL_argerror(L, 1, "Need a zproto_type object");
	luaL_checktype(L, 2, LUA_TTABLE);
	luaL_checkstack(L, ENCODE_DEEPLEVEL * 2 + 8, NULL);
	self.table_index = 2;
	self.tyname = ty->name;
	for (;;){
		self.deep = 0;
		self.iter_index = 0;
		lua_settop(L, self.table_index);
		sz = zproto_encode(ty, header + hlen, size - sz, encode, &self);
		if (sz == ZCB_MEM) {
			buffer = double_buffer(L, &size);
			rev = size >> 3;
			memcpy(buffer + rev, header, hlen);
			header = buffer + rev;
			sz = rev + hlen;
			continue;
		}
		if (sz < 0)
			return luaL_error(L, "Encode err no.%d!", sz);
		sz += hlen;
		break;
	}
	void* tmp = adjust_buffer(L, &size, sz + rev);
	if (tmp){
		buffer = tmp;
		rev = size >> 3;
		memcpy(buffer + rev, header, sz);
	}
	lua_pushlightuserdata(L, buffer + rev);
	lua_pushinteger(L, sz);
	lua_pushlightuserdata(L, buffer);
	lua_pushinteger(L, size);
	return 4;
}

struct decode_ud {
	lua_State* L;
	int array_index;
	int result_index;
	const char* key;
	int deep;
};
static const void* 
getbuffer(lua_State* L, int index, size_t* sz) {
	int t = lua_type(L, index);
	if (t == LUA_TSTRING)
		return lua_tolstring(L, index, sz);
	if (t != LUA_TUSERDATA && t != LUA_TLIGHTUSERDATA)
		luaL_argerror(L, index, "Need a string or userdata");
	*sz = luaL_checkinteger(L, index + 1);
	return lua_touserdata(L, index);
}
static int
decode(struct zproto_arg* args) {
	const struct zfield* f = args->pf;
	struct decode_ud* self = args->ud;
	lua_State* L = self->L;
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
	integer n;
	switch (f->type) {
	case ZT_INTEGER:// notice: in lua 5.2, 52bit integer support (not 64)
		n = *(integer*)args->value;
		if (n & ~0x7fffFFFF)
			lua_pushnumber(L, n);
		else
			lua_pushinteger(L, n);	
		break;
	case ZT_BOOL:
		lua_pushboolean(L, args->value == 0 ? 0 : 1);
		break;
	case ZT_STRING:
		lua_pushlstring(L, args->value, args->length);
		break;
	default:
		lua_newtable(L);
		struct decode_ud sub;
		sub.result_index = lua_gettop(L);
		sub.array_index = sub.result_index + 1;
		sub.deep = self->deep + 1;
		sub.key = NULL;
		sub.L = L;
		lua_pushnil(L);
		const struct ztype* ty = zproto_import(ZP, f->type);
		if (ty == NULL)
			return luaL_error(L, "Invalid type");
		if (args->length != zproto_decode(ty, args->value, args->length, args->shift, decode, &sub))
			return luaL_error(L, "Decode error!");
		lua_settop(L, sub.result_index);
		break;
	}
	if (args->index == 0)
		lua_setfield(L, self->result_index, f->name);
	else if (args->index > 0) {
		if (ZK_ARRAY == f->key)
			lua_seti(L, self->array_index, args->index);
		else if (ZK_MAP == f->key){
			if (*self->key == '\x20')
				lua_seti(L, self->array_index, (int)strtod(self->key + 1, NULL));
			else
				lua_setfield(L, self->array_index, self->key);
		}
	}
	return 0;
}

static int
ldecode_header(lua_State* L) {
	size_t size;
	const char* data = getbuffer(L, 1, &size);
	if (size < 4)
		return luaL_error(L, "Decode error!");
	bool shift = *data != ZP->endian;
	lua_newtable(L);
	lua_pushboolean(L, shift);
	lua_setfield(L, -2, "shift");
	lua_pushinteger(L, (uint16)shift16(data + 2, shift));
	lua_setfield(L, -2, "tag");
	if (*(data + 1) == 0){
		if (size < 8)
			return luaL_error(L, "Decode error!");
		data += 4;
		size -= 4;
		lua_pushnumber(L, (uint32)shift32(data, shift));//session
		lua_setfield(L, -2, "session");
	}
	lua_pushlightuserdata(L, (char*)data + 4);
	lua_pushinteger(L, size - 4);
	return 3;
}

static int
ldecode(lua_State* L) {
	const struct ztype* ty = lua_touserdata(L, 1);
	if (!ty)
		return luaL_argerror(L, 1, "Need a zproto_type object");
	int size;
	const void* data = getbuffer(L, 2, (size_t*)&size);
	bool shift = lua_toboolean(L, 4);
	luaL_checkstack(L, ENCODE_DEEPLEVEL * 3 + 8, NULL);
	if (!lua_istable(L, -1))
		lua_newtable(L);
	struct decode_ud self;
	self.L = L;
	self.key = NULL;
	self.deep = 0;
	self.result_index = lua_gettop(L);
	self.array_index = self.result_index + 1;
	lua_pushnil(L);
	int sz = zproto_decode(ty, data, size, shift, decode, &self);
	if (sz > size || sz < 0)
		return luaL_error(L, "Decode error!");
	lua_settop(L, self.result_index);
	lua_pushlightuserdata(L, (char*)data + sz);
	lua_pushinteger(L, size - sz);
	return 3;
}

static int
lpack(lua_State* L) {
	size_t slen;
	const void* src = getbuffer(L, 1, &slen);
	size_t dlen;
	void* des = (void*)getbuffer(L, 3, &dlen);
	int len = zproto_pack(src, slen, des, dlen);
	lua_pushlstring(L, des, len);
	return 1;
}

static int
lunpack(lua_State* L) {
	int size, sz;
	size_t len;
	const void* data = getbuffer(L, 1, &len);
	void* buffer = lua_touserdata(L, lua_upvalueindex(1));
	size = lua_tointeger(L, lua_upvalueindex(2));
	for (;;) {
		sz = zproto_unpack(data, len, buffer, size);
		if (sz != -1)
			break;
		buffer = double_buffer(L, &size);
	}
	if (size < 0)
		return luaL_error(L, "Unpack data is dirty!!!");
	void* tmp = adjust_buffer(L, &size, sz);
	if (tmp)
		buffer = tmp;
	lua_pushlightuserdata(L, buffer);
	lua_pushinteger(L, sz);
	return 2;
}

static void
pushfunc_withbuffer(lua_State* L, const char* name, lua_CFunction func) {
	lua_newuserdata(L, ENCODE_MINSIZE);
	lua_pushinteger(L, ENCODE_MINSIZE);
	lua_pushinteger(L, 0);
	lua_pushcclosure(L, func, 3);
	lua_setfield(L, -2, name);
}

int
luaopen_zproto_core(lua_State* L) {
#ifdef luaL_checkversion
	luaL_checkversion(L);
#endif
	luaL_Reg l[] = {
		{ "create", lcreate },
		{ "delete", ldelete },
		{ "save", lsave },
		{ "load", lload },
		{ "dump", ldump },
		{ "decode_header", ldecode_header },
		{ "decode", ldecode },
		{ "pack", lpack },
		{ NULL, NULL },
	};
	luaL_newlib(L,l);
	pushfunc_withbuffer(L, "encode", lencode);
	pushfunc_withbuffer(L, "unpack", lunpack);
	return 1;
}
