#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include "msvcint.h"
#include "zproto.h"

#define _shift16(p) (int16)p[1] | (int16)p[0] << 8
#define _shift32(p) (int16)p[3] | (int16)p[2] << 8 | (int32)p[1] << 16 | (int32)p[0] << 24
#define _shift64(p) (int16)p[7] | (int16)p[6] << 8 | (int32)p[5] << 16 | (int32)p[4] << 24 | (int64)p[3] << 32 | (int64)p[2] << 40 | (int64)p[1] << 48 | (int64)p[0] << 56
int16 shift16(const char* p, bool shift){ return shift ? _shift16(p) : *(int16*)p; }
int32 shift32(const char* p, bool shift){ return shift ? _shift32(p) : *(int32*)p; }
int64 shift64(const char* p, bool shift){ return shift ? _shift64(p) : *(int64*)p; }

/**********************memery************************/
#define CHUNK_SIZE 0x100000 //1M

static void
pool_init(struct memery *m, size_t sz) {
	m->size = 0;
	m->ptr = malloc(sz);
}
static void *
pool_alloc(struct memery *m, size_t sz) {
	if (sz == 0)
		return NULL;
	sz = (sz + 3) & ~3;// align by 4
	m->size += sz;
	return (char*)m->ptr + m->size - sz;
}
static void pool_free(struct memery *m) {
	free(m->ptr);
}

/**********************zproto************************/


static char
test_bigendian(){
	union {
		char c;
		short s;
	}u;
	u.s = 1;
	return u.c == 1 ? 0 : 1;
}

void
zproto_init(struct zproto *thiz){
	memset(thiz, 0, sizeof(*thiz));
	thiz->mem.size = (sizeof(*thiz) + 3) & ~3;
}

void *
zproto_alloc(struct zproto *thiz, size_t sz){
	if (sz == 0)
		return NULL;
	sz = (sz + 3) & ~3;// align by 4
	thiz->mem.size += sz;
	return malloc(sz);
}

struct zproto *
zproto_done(struct zproto *thiz){
	int i, j;
	size_t sz = thiz->mem.size;
	pool_init(&thiz->mem, thiz->mem.size);
	struct zproto* z = pool_alloc(&thiz->mem, sizeof(*z));
	memcpy(z, thiz, sizeof(*z));
	z->endian = test_bigendian();
	z->p = pool_alloc(&z->mem, z->pn * sizeof(*z->p));
	if (z->p) {
		memcpy(z->p, thiz->p, z->pn * sizeof(*z->p));
		free(thiz->p);
	}
	z->t = pool_alloc(&z->mem, z->tn * sizeof(*z->t));
	if (z->t) {
		memcpy(z->t, thiz->t, z->tn * sizeof(*z->t));
		for (i = 0; i < thiz->tn; ++i) {
			struct ztype* zt = &z->t[i];
			struct ztype* tt = &thiz->t[i];
			zt->name = pool_alloc(&z->mem, strlen(tt->name) + 1);
			strcpy((char*)zt->name, tt->name);
			zt->f = pool_alloc(&z->mem, tt->n * sizeof(*tt->f));
			if (zt->f) {
				memcpy(zt->f, tt->f, tt->n * sizeof(*tt->f));
				for (j = 0; j < tt->n; ++j) {
					zt->f[j].name = pool_alloc(&z->mem, strlen(tt->f[j].name) + 1);
					strcpy((char*)zt->f[j].name, tt->f[j].name);
					free((void*)tt->f[j].name);
				}
				free(tt->f);
			}
			free((void*)tt->name);
		}
		free(thiz->t);
	}
	memset(thiz, 0, sizeof(*thiz));
	if (sz != z->mem.size)
		return NULL;
	return z;
}

void
zproto_free(struct zproto *thiz){
	pool_free(&thiz->mem);
	thiz = NULL;
}

const struct ztype *
zproto_import(const struct zproto *thiz, int idx) {
	return 0 <= idx && idx < thiz->tn ? &thiz->t[idx] : NULL;
}
const struct zprotocol *
zproto_findtag(const struct zproto *thiz, int tag) {
	int begin = 0, mid, end = thiz->pn, t;
	while (begin < end) {
		mid = (begin + end) / 2;
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
const struct zprotocol *
zproto_findname(const struct zproto *thiz, const char *tyname) {
	const struct ztype *ty;
	int i;
	for (i = 0; i < thiz->pn; ++i) {
		ty = zproto_import(thiz, thiz->p[i].request);
		if (ty && 0 == strcmp(ty->name, tyname))
			return &thiz->p[i];
	}
	return NULL;
}

static const struct zfield *
findtag(const struct ztype *ty, int tag, int *begin) {
	const struct zfield *f;
	int end = ty->n;
	for (; *begin < end; ++(*begin)) {
		f = &ty->f[*begin];
		if (f->tag == tag)
			return f;
	}
	return NULL;//nohere
}

//encode/decode
#define SIZE_HEADER	2
#define SIZE_FIELD	4
#define NULL_CODE 1

static inline int 
encode_tag(int n) {
	assert(0 < n && n <= 0x1fFFffFF);
	return n << 3 | 7;
}
static inline int
encode_len(int n) {
	assert(0 <= n && n <= 0x1fFFffFF);
	return n << 3 | 3;
}
static inline integer
encode_int(integer n) {
	if (n >= 0)
		return (n > 0x7fFFffFF) ? NULL_CODE : (n << 1);
	n = -n;
	return (n > 0x3fFFffFF) ? NULL_CODE : (n << 2 | 1);
}
#define decode_uint(v) (v) >> 1//+
#define decode_int(v) -(int)((v) >> 2)//-
#define decode_tag(v) (v) >> 3
#define decode_len(v) (v) >> 3
static int
encode_key(zproto_cb cb, struct zproto_arg *args, char **buffer, int *size) {
	args->value = *buffer;
	args->length = *size;
	args->index = -args->index;
	int n = cb(args);
	args->index = -args->index;
	if (n > 0) {
		*buffer += n;
		*size -= n;
	}
	return n;
}
#define CHECK_ARRAY_END(sz) if (sz == ZCB_NIL) break; if (sz < 0)	return sz;
static int
encode_int_array(zproto_cb cb, struct zproto_arg *args, char* buffer, int size) {
	char intlen = sizeof(uint32);
	char *header = buffer;
	int sz;
	integer i;
	if (size < 1)
		return ZCB_MEM;
	++buffer;
	--size;
	args->value = &i;
	args->index = 1;
	for (;;) {
		sz = cb(args);
		--args->index;
		CHECK_ARRAY_END(sz);
		if (intlen == sizeof(uint32)) {
			if (sz == sizeof(uint32)) {
				if (intlen > size)
					return ZCB_MEM;
				*(uint32*)(buffer + args->index * sizeof(uint32)) = (uint32)i;
			}
			else {
				intlen = sizeof(uint64);
				sz = args->index * sizeof(uint32);
				if (sz > size)
					return ZCB_MEM;
				size -= sz;
				for (sz = args->index; sz >= 0; --sz)
					*(uint64*)(buffer + sz * sizeof(uint64)) = *(uint32*)(buffer + sz * sizeof(uint32));
			}
		}
		if (intlen == sizeof(uint64)) {
			if (intlen > size)
				return ZCB_MEM;
			*(uint64*)(buffer + args->index * sizeof(uint64)) = i;
		}
		size -= intlen;
		args->index += 2;
	}
	if (args->index == 0)
		return 0;
	*header = intlen;
	return args->index * intlen + 1;
}
static int
encode_int_map(zproto_cb cb, struct zproto_arg *args, char* buffer, int size) {
	char* head = buffer;
	char intlen = sizeof(uint64);
	int sz;
	integer i;
	for (;;) {
		++args->index;
		sz = encode_key(cb, args, &buffer, &size);
		CHECK_ARRAY_END(sz);
		args->value = &i;
		sz = cb(args);
		CHECK_ARRAY_END(sz);
		if (intlen > size)
			return ZCB_MEM;
		*(uint64*)buffer = i;
		buffer += intlen;
		size -= intlen;
	}
	return buffer - head;
}
static int
encode_array(zproto_cb cb, struct zproto_arg *args, char *buffer, int size) {
	const struct zfield* f = args->pf;
	const char* head = buffer;
	int sz;
	switch (f->type) {
	case ZT_INTEGER:
		return f->key == ZK_MAP ? encode_int_map(cb, args, buffer, size) : encode_int_array(cb, args, buffer, size);
		break;
	case ZT_BOOL:
		for (;;) {
			++args->index;
			if (f->key == ZK_MAP) {
				sz = encode_key(cb, args, &buffer, &size);
				CHECK_ARRAY_END(sz);
			}
			sz = cb(args);
			CHECK_ARRAY_END(sz);
			*buffer = args->value == 0 ? 0 : 1;
			--size;
			++buffer;
		}
		break;
	default:
		for (;;) {
			++args->index;
			if (f->key == ZK_MAP) {
				sz = encode_key(cb, args, &buffer, &size);
				CHECK_ARRAY_END(sz);
			}
			if (size < SIZE_HEADER)
				return ZCB_MEM;
			args->value = buffer + SIZE_HEADER;
			args->length = size - SIZE_HEADER;
			sz = cb(args);
			CHECK_ARRAY_END(sz);
			*(uint16*)buffer = sz;
			buffer += SIZE_HEADER + sz;
			size -= SIZE_HEADER + sz;
		}
		break;
	}
	return buffer - head;
}
int
zproto_encode(const struct ztype *ty, char *buffer, int size, zproto_cb cb, void *ud) {
	struct zproto_arg args;
	const struct zfield* f;
	uint32 *fdata = (uint32*)(buffer + SIZE_HEADER);
	int fidx = 0, lasttag = 0, sz = 0;
	int i = SIZE_HEADER + ty->maxn * SIZE_FIELD;
	char *data = buffer + i;
	integer n;
	if (size < i)
		return ZCB_MEM;
	size -= i;
	*(uint16*)buffer = ty->maxn;
	args.ud = ud;
	for (i = 0; i < ty->n; ++i) {
		args.index = 0;
		args.pf = &ty->f[i];
		f = &ty->f[i];
		if (f->tag != (lasttag + 1))
			fdata[fidx++] = encode_tag(f->tag - lasttag - 1);
		if (f->key == ZK_NULL) {
			switch (f->type) {
			case ZT_INTEGER:
				args.value = &n;
				sz = cb(&args);
				if (sz < 0)
					return sz;
				fdata[fidx] = (uint32)encode_int(n);
				if (fdata[fidx] == NULL_CODE) {
					sz = sizeof(integer);
					if (sz > size)
						return ZCB_MEM;
					fdata[fidx] = encode_len(sz);
					*(integer*)data = n;
				}
				break;
			case ZT_BOOL:
				sz = cb(&args);
				fdata[fidx] = (uint32)encode_int((size_t)args.value);
				break;
			default:
				args.value = data;
				args.length = size;
				sz = cb(&args);
				fdata[fidx] = encode_len(sz);
				break;
			}
		}
		else if (f->key == ZK_ARRAY || f->key == ZK_MAP){
			sz = encode_array(cb, &args, data, size);
			fdata[fidx] = encode_len(sz);
			--args.index;
		}
		else
			return ZCB_ERR;
		if (sz < 0)
			return ZCB_MEM;
		data += sz;
		size -= sz;
		++fidx;
		lasttag = f->tag;
	}
	if (fidx != ty->maxn)
		return ZCB_ERR;
	return data - buffer;
}

static int
decode_key(zproto_cb cb, struct zproto_arg *args, const char **stream, int* size){
	int n = 1 + strlen(*stream);
	if (*size < n)
		return ZCB_MEM;
	args->value = (void*)*stream;
	args->index = -args->index;
	cb(args);
	args->index = -args->index;
	*stream += n;
	*size -= n;
	return n;
}
static int
decode_array(zproto_cb cb, struct zproto_arg *args, const char* stream, int size) {
	const struct zfield *f = args->pf;
	int sz = size;
	int n;
	switch (f->type) {
	case ZT_INTEGER:
		if (f->key == ZK_MAP)
			n = 8;
		else {
			n = (*stream++);
			if (--sz < 0 || (n != 8 && n != 4))
				return ZCB_ERR;
		}
		integer intv;
		for (; sz > 0; sz -= n, stream += n) {
			++args->index;
			if (f->key == ZK_MAP && decode_key(cb, args, &stream, &sz) < 0)
				return ZCB_MEM;
			intv = n == 4 ? shift32(stream, args->shift) : shift64(stream, args->shift);
			args->value = &intv;
			cb(args);
		}
		break;
	case ZT_BOOL:
		for (; sz > 0; --sz, ++stream) {
			++args->index;
			if (f->key == ZK_MAP && decode_key(cb, args, &stream, &sz) < 0)
				return ZCB_MEM;
			if (sz < 1)
				return ZCB_MEM;
			args->value = (void*)*stream;
			cb(args);
		}
		break;
	default:
		if (f->type != ZT_STRING && f->type < 0)
			return ZCB_ERR;
		for (; sz > 0; sz -= n, stream += n) {
			++args->index;
			if (f->key == ZK_MAP && decode_key(cb, args, &stream, &sz) < 0)
				return ZCB_MEM;
			if (sz < SIZE_HEADER)
				return ZCB_MEM;
			args->value = (void*)(stream + SIZE_HEADER);
			args->length = shift16(stream, args->shift);
			n = args->length + SIZE_HEADER;
			if (sz < n)
				return ZCB_MEM;
			cb(args);
		}
	}	
	return size - sz;
}
int
zproto_decode(const struct ztype *ty, const char *data, int size, bool shift, zproto_cb cb, void *ud) {
	const char *streamf, *streamd;
	struct zproto_arg args;
	int i, tag, sz, begin;
	const struct zfield *f;
	integer intv;
	uint16 fn;
	uint32 val;
	args.ud = ud;
	args.shift = shift;
	if (size < SIZE_HEADER)
		return ZCB_MEM;
	size -= SIZE_HEADER;
	fn = shift16(data, args.shift);
	sz = fn * SIZE_FIELD;
	if (size < sz)
		return ZCB_MEM;
	size -= sz;
	streamf = data + SIZE_HEADER;
	streamd = streamf + sz;
	tag = 0;
	begin = 0;
	for (i = 0; i < fn ; ++i) {
		val = shift32(streamf + i * SIZE_FIELD, args.shift);
		if ((val & 7) == 7) {//tag
			tag += decode_tag(val);
			continue;
		}
		sz = ((val & 7) == 3) ? decode_len(val) : 0;//len
		if (size < sz)
			return ZCB_MEM;
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
				if ((val & 1) == 0)//+
					intv = decode_uint(val);
				else if ((val & 3) == 1)//-
					intv = decode_int(val);
				else {//ext
					if (sz != sizeof(integer))
						return ZCB_ERR;
					intv = shift64(streamd, args.shift);
				}
				args.value = &intv;
				break;
			case ZT_BOOL:
				args.value = (void*)(decode_uint(val));
				break;
			default:
				args.value = (void*)streamd;
				args.length = sz;
				break;
			}
			cb(&args);
		}
		else if (sz > 0 && sz != decode_array(cb, &args, streamd, sz))
			return ZCB_MEM;
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
zproto_pack(const char* data, int size, char *buffer, int len) {
	char *ffp = NULL, *buf = buffer;
	int i, n, ffn = 0;
	for (i = 0; i < size; i += 8) {
		if (ffn == 0) {
			n = pack_seg(data, size - i, buf, len);
			if (n < 0)
				return -1;
			if (n == 9)	{
				if (len < 10)
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
			if (len < 8)
				return -1;
			n = pack_seg_ff(data, size - i, buf);
			if (n >= 6 && ffn < 256){
				*ffp = ffn;
				++ffn;
				n = 8;
			}
			else {
				ffn = 0;
				n = pack_seg(data, size - i, buf, len);
			}
		}
		data += 8;
		buf += n;
		len -= n;
	}
	return buf - buffer;
}
int
zproto_unpack(const char *data, int size, char *buffer, int len) {
	unsigned char seg0;
	char *buf = buffer;
	int n;
	while (size-- > 0) {
		seg0 = *data++;
		if (seg0 == 0xFF) {
			if (--size < 0)
				return -2;
			n = ((unsigned char)*data + 1) << 3;
			if (size < n)
				return -2;
			if (len < n)
				return -1;
			memcpy(buf, ++data, n);
			size -= n;
			len -= n;
			buf += n;
			data += n;
		}
		else {
			for (n = 0; n < 8; ++n) {
				if (--len < 0)
					return -1;
				if ((seg0 >> n) & 1) {
					if (--size < 0)
						return -2;
					*buf++ = *data++;
				}
				else
					*buf++ = 0;
			}
		}
	}
	return buf - buffer;
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
