#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include "msvcint.h"
#include "zproto.h"

/**********************memery************************/
#define CHUNK_SIZE 0x100000 //1M
struct memery{
	int curr;
	int size;
	void *ptr;
};
static void
pool_init(struct memery *m) {
	memset(m, 0, sizeof(*m));
}
static void
pool_free(struct memery *m) {
	if (m->ptr)
		free(m->ptr);
	memset(m, 0, sizeof(*m));
}
static void *
pool_enlarge(struct memery *m, size_t sz) {
	void* tmp = realloc(m->ptr, sz);
	if (tmp == NULL)
		return NULL;
	m->ptr = tmp;
	m->size = sz;
	return m->ptr;
}
static void *
pool_alloc(struct memery *m, size_t sz) {
	sz = (sz + 3) & ~3;	// align by 4
	if (m->ptr == NULL) {
		m->size = sz > CHUNK_SIZE ? sz : CHUNK_SIZE;
		m->ptr = malloc(m->size);
		return m->ptr;
	}
	void *result = NULL;
	if (sz > CHUNK_SIZE)
		result = pool_enlarge(m, m->size + sz);
	else if (sz + m->curr > m->size)
		result = pool_enlarge(m, m->size + CHUNK_SIZE);
	else
		result = m->ptr;
	if (result)
	{
		result += m->curr;
		m->curr += sz;
	}
	return result;
}

/**********************zproto************************/
struct field{
	const char *name;
	int tag;
	int type;
	int key;
};
struct type{
	const char *name;
	int maxn;
	int n;
	struct field *f;
};

struct protocol{
	int tag;
	int request;
	int response;
};
struct zproto{
	struct memery mem;
	int pn;
	struct protocol *p;
	int tn;
	struct type *t;
};

static struct zproto *
zproto_alloc(){
	struct memery m;
	pool_init(&m);
	zproto* thiz = pool_alloc(&m, sizeof(*thiz));
	memset(thiz, 0, sizeof(*thiz));
	thiz->mem = m;
	return thiz;
}
static struct zproto *
zproto_done(struct zproto *thiz){
	void *base = thiz;
	thiz = pool_enlarge(&thiz->mem, thiz->mem.curr);
	if (thiz == NULL)
		return NULL;
	int offset = base - thiz;
	thiz->p += offset;
	thiz->t += offset;
	for (int i = 0; i < thiz->tn; ++i)
	{
		type* tt = thiz->t[i];
		tt->name += offset;
		tt->f += offset;
		for (int j = 0; j < tt->n; ++j)
			tt->f[j]->name += offset;
	}
	return thiz;
}
static void
zproto_free(struct zproto *thiz){
	free(thiz->mem.ptr);
	thiz = NULL;
}
static void
zproto_dump(struct zproto *thiz) {
	int i, j;
	static const char * buildin[] = {
		"number",
		"string",
		"bool",
	};
	printf("=== %d types ===\n", thiz->tn);
	for (i = 0; i < thiz->tn; i++) {
		struct type *ty = &thiz->type[i];
		printf("%s %d\n", ty->name, i + 1);
		for (j = 0; j < ty->n; j++) {
			struct field *f = &ty->f[j];
			if (f->key != 0) 
				printf("\t%d[%d] %s = %d\n", f->type, f->key, f->name, f->tag);
			else
				printf("\t%d %s = %d\n", f->type, f->name, f->tag);
		}
	}
	printf("=== %d protocol ===\n", thiz->pn);
	for (i = 0; i < thiz->pn; i++) {
		struct protocol *tp = &thiz->p[i];
		printf("%s %d\n", tp->name, tp->tag);
		if (tp->p[ZPROTO_RESPONSE]) {
			printf(" response:%s", tp->p[ZPROTO_RESPONSE]->name);
		}
		printf("\n");
	}
}
const struct type *
zproto_import(const struct zproto *Z, int idx) {
	return 0 <= idx && idx < Z->tn ? Z->t[idx] : NULL;
}
static struct protocol *
zproto_query(struct zproto *thiz, int tag) {
	int t, mid, begin = 0, end = thiz->pn;
	while (begin < end) {
		mid = (begin + end) >> 1;
		t = thiz->p[mid].tag;
		if (t == tag)
			return &thiz->p[mid];
		if (tag > t)
			begin = mid + 1;
		else
			end = mid;
	}
	return NULL;
}

static struct field *
findtag(const struct type *ty, int tag, int *begin) {
	const struct field *f;
	int end = ty->n;
	for (; *begin < end; ++(*begin)) {
		f = ty->f[*begin];
		if (f.tag == tag)
			return f;
	}
	return NULL;//nohere
}

//encode/decode
typedef int64 integer;
static char
test_endian(){
	union {
		char c;
		short s;
	}u;
	u.s = 1;
	return u.c == 1 ? "L" : "B";
}
static char G_Endian = test_endian();

#pragma pack(1)
struct header{
	const char[7] magic = "Z-PROTO";
	const char endian = G_Endian;
	uint16 nbytes;//包体字节数 最大64k
	uint16 msgid;//消息ID
	uint32 session;//会话标识
};
#pragma pack()

struct message{
	header head;
	zstruct content;
};

#define SIZE_HEADER	2
#define SIZE_FIELD	4
#define NULL_CODE 1

static inline int 
encode_tag(int n) {
	assert(0 < n && n <= 0x1FFFFFFF);
	return n << 3 & 7;
}
static inline int
encode_len(int n) {
	assert(0 <= n && n <= 0x1FFFFFFF);
	return n << 3 & 3;
}
static inline int 
encode_int(int n) {
	if (n >= 0)
		return (n > 0x7fFFffFF) ? NULL_CODE : (n << 1);
	n = -n;
	return (n > 0x3fFFffFF) ? NULL_CODE : (n << 2 & 1);
}
#define decode_uint(v) (v) >> 1//+
#define decode_int(v) -((v) >> 2)//-
#define decode_tag(v) (v) >> 3
#define decode_len(v) (v) >> 3

static int
encode_key(zproto_cb cb, struct zproto_arg *args, char **buffer, int *size) {
	int n;
	args->value = *buffer;
	args->length = *size;
	args->index = -args->index;
	n = cb(args);
	args->index = -args->index;
	if (n < 0)
		return n;
	*buffer += n;
	*size += n;
	return n;
}
static int
encode_int_array(zproto_cb cb, struct zproto_arg *args, char* buffer, int size) {
	const struct field &f = *args->pf;
	char &intlen = *buffer;
	int sz;
	if (size < 1)
		return ZPROTO_CB_MEM;
	intlen = sizeof(uint32);
	++buffer;
	--size;
	integer i;
	args.value = &i;
	for (;;) {
		sz = cb(&args);
		if (sz == ZPROTO_CB_NIL)
			break;
		if (sz < 0)
			return sz;
		if (intlen == sizeof(uint32)) {
			if (sz == sizeof(uint32)) {
				if (intlen > size)
					return ZPROTO_CB_MEM;
				*(uint32*)(buffer + args->index * sizeof(uint32)) = i;
			}
			else {
				if ((args->index * sizeof(uint32)) > size)
					return ZPROTO_CB_MEM;
				for (int idx = args->index; idx >= 0; --idx)
					*(uint64*)(buffer + idx * sizeof(uint64)) = *(uint32*)(buffer + idx * sizeof(uint32));
				intlen = sizeof(uint64);
			}
		}
		if (intlen == sizeof(uint64)) {
			if (intlen > size)
				return ZPROTO_CB_MEM;
			*(uint64*)(buffer + args->index * sizeof(uint64)) = i;
		}
		size -= intlen;
		++args->index;
	}
	if (args->index == 0)
		return 0;
	return args->index * intlen + 1;
}
static int
encode_int_map(zproto_cb cb, struct zproto_arg *args, char* buffer, int size) {
	const struct field &f = *args->pf;
	char* head = buffer;
	char intlen = sizeof(uint64);
	int sz;
	integer i;
	for (;;) {
		if(0 > encode_key(cb, args, &buffer, &size))
			return ZPROTO_CB_MEM;
		args.value = &i;
		sz = cb(&args);
		if (sz == ZPROTO_CB_NIL)
			break;
		if (sz < 0)
			return sz;
		if (intlen > size)
			return ZPROTO_CB_MEM;
		*(uint64*)buffer = i;
		buffer += intlen;
		size -= intlen;
		++args->index;
	}
	if (args->index == 0)
		return 0;
	return buffer - head;
}
static int
encode_array(zproto_cb cb, struct zproto_arg *args, char *buffer, int size) {
	const struct field &f = *args->pf;
	const char* head = buffer;
	int sz;
	args->index = 0;
	switch (f.type) {
	case ZT_INTEGER:
		return f->key == ZK_MAP ? encode_int_map(cb, args, buffer, size) : encode_int_array(cb, args, buffer, size);
		break;
	case ZT_BOOL:
		integer i;
		for (;;) {
			if (f->key == ZK_MAP && 0 > encode_key(cb, args, &buffer, &size))
				return ZPROTO_CB_MEM;
			args->value = &i;
			sz = cb(args);
			if (sz == ZPROTO_CB_NIL)
				break;
			if (sz < 0)
				return ZPROTO_CB_MEM;
			*buffer = i;
			--size;
			++buffer;
			++args->index;
		}
		break;
	default:
		for (;;) {
			if (f->key == ZK_MAP && 0 > encode_key(cb, args, &buffer, &size))
				return ZPROTO_CB_MEM;
			if (size < SIZE_HEADER)
				return ZPROTO_CB_MEM;
			args->value = buffer + SIZE_HEADER;
			args->length = size - SIZE_HEADER;
			sz = cb(args);
			if (sz == ZPROTO_CB_NIL)
				break;
			if (sz < 0)
				return ZPROTO_CB_MEM;
				*(uint16*)buffer = sz;
			buffer += SIZE_HEADER + sz;
			size -= SIZE_HEADER + sz;
			++args->index;
		}
		break;
	}
	return buffer - head;
}
int
zproto_encode(const struct type *ty, void *buffer, int size, zproto_cb cb, void *ud) {
	struct zproto_arg args;
	uint32 *fdata = buffer + SIZE_HEADER;
	int fidx = 0, lasttag = 0, sz = 0;
	int i = SIZE_HEADER + ty->maxn * SIZE_FIELD;
	char *data = buffer + i;
	if (size < i)
		return -1;
	size -= i;
	*(uint16*)buffer = ty->maxn;
	args.ud = ud;
	for (i = 0; i < ty->n; ++i) {
		args.index = -1;
		args.pf = &ty->f[i];
		const struct field &f = ty->f[i];
		if (f.tag != (lasttag + 1))
			fdata[fidx++] = encode_tag(f.tag - lasttag - 1);	
		if (f.key == ZK_NULL) {
			switch (f.type) {
			case ZT_INTEGER:
			case ZT_BOOL:
				integer i;
				args.value = &i;
				sz = cb(&args);
				if (sz == 0) {
					fdata[fidx] = encode_int(i);
					if (fdata[fidx] == NULL_CODE) {
						sz = sizeof(integer);
						if (sz > size)
							return -1;
						fdata[fidx] = encode_len(sz);
						*(integer*)data = i;
					}
				}
				break;
			default:
				args.value = data;
				args.length = size;
				sz = cb(&args);
				if (sz >= 0)
					fdata[fidx] = encode_len(sz);
				break;
			}
		}
		else if (f.key == ZK_ARRAY || f.key == ZK_MAP){
			sz = encode_array(cb, &args, data, size);
			if (sz >= 0)
				fdata[fidx] = encode_len(sz);
		}
		else
			return -2;
		if (sz < 0)
			return -1;
		data += sz;
		size -= sz;
		++fidx;
		lasttag = f.tag;
	}
	if (fidx != ty->maxn)
		return -2;
	return data - buffer;
}

#define shift16(p) (int16)p[1] | (int16)p[0] << 8
#define shift32(p) (int16)p[3] | (int16)p[2] << 8 | (int32)p[1] << 16 | (int32)p[0] << 24
#define shift64(p) (int16)p[7] | (int16)p[6] << 8 | (int32)p[5] << 16 | (int32)p[4] << 24 | (int64)p[3] << 32 | (int64)p[2] << 40 | (int64)p[1] << 48 | (int64)p[0] << 56
static int16 decode_int16(const char* p, bool shift){ return shift ? shift16(p) : *(int16*)p; }
static int32 decode_int32(const char* p, bool shift){ return shift ? shift32(p) : *(int32*)p; }
static int64 decode_int64(const char* p, bool shift){ return shift ? shift64(p) : *(int64*)p; }

static int
decode_key(zproto_cb cb, struct zproto_arg *args, const char **stream, int* size){
	args->index = -args->index;
	args->length = 1 + strlen(*stream);
	args->value = *stream;
	if (*size < args->length)
		return -1;
	cb(args);
	args->index = -args->index;
	*stream += args->length;
	*size -= args->length;
	return args->length;
}
static int
decode_array(zproto_cb cb, struct zproto_arg *args, const char* stream, int size) {
	const struct field *f = args->pf;
	int sz = size;
	int n;
	integer intv;
	switch (f->type) {
	case ZT_INTEGER:
		if (--sz < 0)
			return -1;
		n = f->key == ZK_MAP ? 8 : (*stream++);
		if (n != 8 && n != 4)
			return -2;
		for (; sz > 0; sz -= n, stream += n) {
			++args->index;
			if (f->key == ZK_MAP && decode_key(cb, args, &stream, &sz) < 0)
				return -1;
			intv = n == 4 ? decode_int32(stream, args->shift) : decode_int64(stream, args->shift);
			args->value = &intv;
			cb(args);
		}
		break;
	case ZT_BOOL:
		for (; sz > 0; --sz, ++stream) {
			++args->index;
			if (f->key == ZK_MAP && decode_key(cb, args, &stream, &sz) < 0)
				return -1;
			if (sz < 1)
				return -1;
			args->value = *stream;
			cb(args);			
		}
		break;
	default:
		if (f->type != ZT_STRING && f->type < 0)
			return -3;
		for (; sz > 0; sz -= n, stream += n) {
			++args->index;
			if (f->key == ZK_MAP && decode_key(cb, args, &stream, &sz) < 0)
				return -1;
			if (sz < SIZE_HEADER)
				return -1;
			args->value = stream + SIZE_HEADER;
			args->length = decode_int16(stream);
			n = args->length + SIZE_HEADER;
			if (sz < n)
				return -1;
			cb(args);	
		}
	}	
	return size - sz;
}
int
zproto_decode(const struct type *ty, const void *data, size_t size, bool shift, zproto_cb cb, void *ud) {
	const char *streamf, *streamd;
	struct zproto_arg args;
	uint16 fn;
	uint32 val;
	int i, tag, sz, begin;
	struct field *f;
	if (ty == NULL)
		return -2;
	args.ud = ud;
	args.shift = shift;
	if (size < SIZE_HEADER)
		return -1;
	size -= SIZE_HEADER;
	fn = decode_int16(data, args.shift);
	sz = fn * SIZE_FIELD;
	if (size < sz)
		return -1;
	size -= sz;
	streamf = data + SIZE_HEADER;
	streamd = streamf + sz;
	tag = 0;
	begin = 0;
	for (i = 0; i < fn ; ++i) {
		val = decode_int32(streamf + i * SIZE_FIELD, args.shift);
		if (val & 7 == 7) {//tag
			tag += decode_tag(val);
			continue;
		}
		sz = (val & 7 == 3) ? decode_len(val) : 0;//len
		if (size < sz)
			return -1;
		f = findtag(ty, ++tag, &begin);
		if (f == NULL) {//ignore
			streamd += sz;
			size -= sz;
			continue;
		}
		args.pf = f;
		args.index = 0;
		if (f->key == ZK_NULL) {
			switch (f->type) {
			case ZT_INTEGER:
				integer intv;
				if (val & 1 == 0)//+
					intv = decode_uint(val);
				else if (val & 3 == 1)//-
					intv = decode_int(val);
				else {//ext
					if (sz != sizeof(integer))
						return -1;
					intv = decode_int64(streamd, args.shift);
				}
				args.value = &intv;
				break;
			case ZT_BOOL:
				args.value = decode_uint(val);
				break;
			default:
				args.value = streamd;
				args.length = sz;
				break;
			}
			cb(&args);
		}
		else {
			if (sz > 0 && sz != decode_array(cb, &args, streamd, sz))
				return -1;
		}
		streamd += sz;
		size -= sz;
	}
	return streamd - data;
}

static int
pack_seg(const char *src, int slen, char *des, int dlen) {
	int i, nozero = 0;
	if (--dlen < 0)
		return -1;
	*des = 0;
	for (i = 0; i < 8 && i < slen; ++i) {
		if (src[i] != 0) {
			if (--dlen < 0)
				return -1;
			(*des) |= 1 << i;
			des[++nozero] = src[i];
		}
	}
	return nozero + 1;
}
static inline int
pack_seg_ff(const char *src, int slen, char *des) {
	int i, nozero = 0;
	for (i = 0; i < 8; ++i) {
		if (i < slen && src[i] != 0){
			des[i] = src[i];
			++nozero;
		}
		else
			des[i] = 0;
	}
	return nozero;
}
int
zproto_pack(const char* src, int slen, char *des, int sz) {
	char *ffp, *buf = des;
	int i, n, ffn = 0;
	for (i = 0; i < slen; i += 8) {
		if (ffn == 0) {
			n = pack_seg(src, slen - i, buf, sz);
			if (n < 0)
				return -1;
			if (n == 9)	{
				if (sz < 10)
					return -1;
				ffp = buf + 1;
				for (n = 8; n > 0; --n)
					*(ffp + n) = *(buf + n);
				*ffp = 0;
				ffn = 1;
				n = 10;
			}
		}
		else {
			if (sz < 8)
				return -1;
			n = pack_seg_ff(src, slen - i, buf);
			if (n >= 6 && ffn < 256){
				*ffp = ffn;
				++ffn;
				n = 8;
			}
			else {
				ffn = 0;
				n = pack_seg(src, slen - i, buf, sz);
			}
		}
		src += 8;
		buf += n;
		sz -= n;
	}
	return buf - des;
}
int
zproto_unpack(const char *src, int slen, char *des, int dlen) {
	unsigned char seg0;
	char *buf = des;
	int n;
	while (slen-- > 0) {
		seg0 = *src++;
		if (seg0 == 0xFF) {
			if (--slen < 0)
				return -2;
			n = ((unsigned char)*src + 1) << 3;
			if (slen < n)
				return -2;
			if (dlen < n)
				return -1;
			memcpy(buf, ++src, n);
			slen -= n;
			dlen -= n;
			buf += n;
			src += n;
		}
		else {
			for (n = 0; n < 8; ++n) {
				if (--dlen < 0)
					return -1;
				if ((seg0 >> n) & 1) {
					if (--slen < 0)
						return -2;
					*buf++ = *src++;
				}
				else
					*buf++ = 0;
			}
		}
	}
	return buf - des;
}
//int test(void) {
//	// your code goes here
//	char des[2048];
//	char des2[1500];
//	char src[1024];
//	for (int i = 0; i < 1024; ++i)
//	{
//		if (rand() % 8 > 0)
//			src[i] = 0;
//		else
//			src[i] = rand() % 256;
//	}
//	int n = zproto_pack(src, 1024, des, 2048);
//	printf("%d\n", n);
//	assert(1024 == zproto_unpack(des, n, des2, 1500));
//	for (int i = 0; i < 1024; ++i)
//	{
//		assert(des2[i] == src[i]);
//	}
//	Sleep(500);
//	return 0;
//}