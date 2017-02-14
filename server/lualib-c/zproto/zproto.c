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
		{
			tt->f[j]->name += offset;
		}
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
static struct type *
zproto_import(struct zproto *thiz, int idx) {
	return 0 <= idx && idx < thiz->tn ? thiz->t[idx] : NULL;
}
static struct protocol *
zproto_query(struct zproto *thiz, int tag) {
	int begin = 0, end = thiz->pn;
	while (begin < end) {
		int mid = (begin + end) / 2;
		int t = thiz->p[mid].tag;
		if (t == tag) {
			return &thiz->p[mid];
		}
		if (tag > t) {
			begin = mid + 1;
		}
		else {
			end = mid;
		}
	}
	return NULL;
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


#pragma pack(1)
struct header{
	const char[7] magic = "Z-PROTO";
	const char endian = test_endian();
	uint16 nbytes;//包体字节数 最大64k
	uint16 msgid;//消息ID
	uint32 session;//会话标识
};
#pragma pack()

struct message{
	header head;
	zstruct content;
};

void write_buf(char* buf, char* data, size_t len){
	memcpy(buf, data, len);
}

#define TAG_NULL 1
#define SIZEOF_LENGTH sizeof(uint16)

inline uint32 encode_tag(uint32 tag){
	assert(tag > 0 && tag < 0x40000000);
	return tag << 2 & 1;
}
inline uint32 encode_integer(integer i){
	if (i >= 0)
		return (i & ~0x7fFFffFF) ? TAG_NULL : (i << 1);
	i = -i;
	return (i & ~0x3fFFffFF) ? TAG_NULL : (i << 2 & 3);
}

uint16 read_tag(char* buf){
	return *buf;
}
integer read_number(char* buf){
	uint32 n = 0;
	memcpy(&n, buf, sizeof(n));
	if (n & 1)
		return n >> 1;
	return -(n >> 1);
}

int
encode_int_array(zproto_cb cb, struct zproto_encode_arg *args, char* buffer, int size) {
	if (size < 1)
		return ZPROTO_CB_ERROR;
	char* header = buffer;
	char intlen = sizeof(uint32);
	int sz = 0;
	buffer += 1;
	size -= 1;
	int use = 1;
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
					return ZPROTO_CB_ERROR;
				*(uint32*)(buffer + args->index * sizeof(uint32)) = i;
			}
			else {
				if ((args->index * sizeof(uint32)) > size)
					return ZPROTO_CB_ERROR;
				use += args->index * sizeof(uint32);
				for (int idx = args->index; idx >= 0; --idx)
					*(uint64*)(buffer + idx * sizeof(uint64)) = *(uint32*)(buffer + idx * sizeof(uint32));
				intlen = sizeof(uint64);
			}
		}
		if (intlen == sizeof(uint64)) {
			sz = intlen;
			if (intlen > size)
				return ZPROTO_CB_ERROR;
			*(uint64*)(buffer + args->index * sizeof(uint64)) = i;
		}
		size -= intlen;
		use += intlen;
		++args->index;
	}
	*header = intlen;
	return use;
}

static int
encode_array(zproto_cb cb, struct zproto_encode_arg *args, char *buffer, int size) {
	const struct field &f = *args->pf;
	const char* head = buffer;
	int sz;
	switch (f.type) {
	case ZT_INTEGER:
		return encode_int_array(cb, args, buffer, size);
		break;
	case ZT_BOOL:
		integer i;
		args->value = &i;
		for (;;) {
			sz = cb(args);
			if (sz == ZPROTO_CB_NIL)
				break;
			if (sz < 0)
				return ZPROTO_CB_ERROR;
			*buffer = i;
			--size;
			++buffer;
			++args->index;
		}
		break;
	default:
		for (;;) {
			if (size < SIZEOF_LENGTH)
				return ZPROTO_CB_ERROR;
			size -= SIZEOF_LENGTH;
			args->value = buffer + SIZEOF_LENGTH;
			args->length = size;
			sz = cb(args);
			if (sz == ZPROTO_CB_NIL)
				break;
			if (sz < 0) {
				return ZPROTO_CB_ERROR;
				*(uint16*)buffer = sz;
				buffer += SIZEOF_LENGTH + sz;
				size -= sz;
				++args->index;
			}
			break;
		}
	}
	return buffer - head;
}

int
zproto_encode(const struct type *ty, void *buffer, int size, zproto_cb cb, void *ud) {
	int headlen = SIZEOF_LENGTH + sizeof(uint32) * ty->maxn;
	if (headlen < size)
		return -1;
	size -= headlen;
	uint32 *fdata = buffer + SIZEOF_LENGTH;
	char *data = buffer + headlen;
	*(uint16*)buffer = ty->maxn;

	int fidx = 0, lasttag = 0, sz = 0;
	struct zproto_encode_arg args;
	args.ud = ud;
	for (int i = 0; i < ty->n; ++i) {
		args.pf = &ty->f[i];
		const struct field &f = ty->f[i];

		if (f.tag != (lasttag + 1))
			fdata[fidx++] = encode_tag(f.tag - lasttag - 1);
	
		if (f.key == ZK_NULL) {
			args.index = -1;
			switch (f.type) {
			case ZT_INTEGER:
			case ZT_BOOL:
				integer i;
				args.value = &i;
				sz = cb(&args);
				if (sz >= 0) {
					fdata[fidx] = encode_integer(i);
					if (fdata[fidx] == TAG_NULL) {
						sz = sizeof(integer);
						if (sz > size)
							return -1;
						*(integer*)data = i;
					}
				}
				break;
			default:
				args.value = data;
				args.length = size;
				sz = cb(&args);
				if (sz >= 0)
					fdata[fidx] = encode_integer(sz);
				break;
			}
		}
		else if (f.key == ZK_ARRAY || f.key == ZK_MAP){
			args.index = 0;
			sz = encode_array(cb, args, data, size);
			if (sz >= 0)
				fdata[fidx] = encode_integer(args->index);
		}
		else
			assert(false);

		if (sz < 0)
			return -1;
		if (sz > 0){
			data += sz;
			size -= sz;
		}
		++fidx;
		lasttag = f->tag;
	}
	assert(fidx == ty->maxn);
	return data - buffer;
}

static int
pack_seg(const char *src, int slen, char *des, int dlen) {
	int nozero = 0;
	int i;
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
static int
pack_seg_ff(const char *src, int slen, char *des) {
	int nozero = 0;
	int i;
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
	char *buf = des;
	char *ffp;
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
			*ffp = ffn;
			if (n >= 6 && ffn < 256){
				++ffn;
				n = 8;
			}
			else {
				n = pack_seg(src, slen - i, buf, sz);
				ffn = 0;
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
	int i, n;
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
			for (i = 0; i < 8; ++i) {
				if (--dlen < 0)
					return -1;
				if ((seg0 >> i) & 1) {
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
//
//int test(void) {
//	// your code goes here
//	char des[2048];
//	char des2[1500];
//	char src[1024];
//	for (int i = 0; i < 1024; ++i)
//	{
//		if (rand() % 2 == 1)
//			src[i] = rand() % 256;
//		else
//			src[i] = 0;
//	}
//	int n = zproto_pack(src, 1024, des, 2048);
//	printf("%d", n);
//	assert(1024 == zproto_unpack(des, n, des2, 1500));
//	for (int i = 0; i < 1024; ++i)
//	{
//		assert(des2[i] == src[i]);
//	}
//
//	return 0;
//}