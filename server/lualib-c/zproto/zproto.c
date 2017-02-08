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

inline void encode_size(){
	uint16 len = strlen(cstr);
	write_buf(buf, &len, sizeof(len));
	write_buf(buf, cstr, len + 1);
}
void encode_struct(zstruct* content, char* buf){
	write_buf(buf, content->n, sizeof(content->n));
	encode_tag(buf + 2, content->n, sizeof(content->n));
}
void encode_message(char* buf, size_t s, message* msg){
	write_buf(buf, msg->head, sizeof(msg->head));
	encode_struct(buf, msg->content);
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
encode_integer_array(zproto_cb cb, struct zproto_encode_arg *args, char* buffer, int size) {
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
		if (sz >= 0) {
			if (intlen == sizeof(uint32)) {
				if (sz == sizeof(uint32)) {
					if (intlen > size)
						return -1;
					*(uint32*)(buffer + (args->index - 1) * sizeof(uint32) ) = i;
				}
				else {
					if (((args->index - 1) * sizeof(uint32))> size)
						return -1;
					use += (args->index - 1) * sizeof(uint32);
					for (int idx = args->index - 1; idx > 0; --idx)
						*(uint64*)(buffer + idx * sizeof(uint64)) = *(uint32*)(buffer + idx * sizeof(uint32));
					intlen = sizeof(uint64);
				}
			}
			if (intlen == sizeof(uint64)) {
				sz = intlen;
				if (intlen > size)
					return -1;
				*(uint64*)(buffer + (args->index - 1) * sizeof(uint64)) = i;
			}
		}
		size -= intlen;
		use += intlen;
		++args->index;
	}
	*header = intlen;
	return use;
}

static int
encode_array(zproto_cb cb, struct zproto_encode_arg *args, char *data, int size) {
	int sz = 0;
	const struct field &f = *args->pf;
	switch (f.type) {
	case ZT_INTEGER:
		encode_integer_array(cb, args, data, size);
		break;							
	case ZT_BOOL:
		for (;;) {
			int v = 0;
			args->value = &v;
			args->length = sizeof(v);
			sz = cb(args);
			if (sz < 0) {
				if (sz == SPROTO_CB_NIL)		// nil object , end of array
					break;
				if (sz == SPROTO_CB_NOARRAY)	// no array, don't encode it
					return 0;
				return -1;	// sz == SPROTO_CB_ERROR
			}
			if (size < 1)
				return -1;
			buffer[0] = v ? 1 : 0;
			size -= 1;
			buffer += 1;
			++args->index;
		}
		break;
	default:
		args->index = 1;
		for (;;) {
			if (size < SIZEOF_LENGTH)
				return -1;
			size -= SIZEOF_LENGTH;
			args->value = buffer + SIZEOF_LENGTH;
			args->length = size;
			sz = cb(args);
			if (sz < 0) {
				if (sz == SPROTO_CB_NIL) {
					break;
				}
				if (sz == SPROTO_CB_NOARRAY)	// no array, don't encode it
					return 0;
				return -1;	// sz == SPROTO_CB_ERROR
			}
			fill_size(buffer, sz);
			buffer += SIZEOF_LENGTH + sz;
			size -= sz;
			++args->index;
		}
		break;
	}
	sz = buffer - (data + SIZEOF_LENGTH);
	return fill_size(data, sz);
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
			args.index = 0;
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
			args.index = 1;
			sz = encode_array(cb, args, data, size);
			if (sz >= 0)
				fdata[fidx] = encode_integer(args->length);
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

// 0 pack
static int
pack_seg(const char *src, char *buf, int sz, int n) {
	char header = 0;
	int notzero = 0;
	int i;
	char *obuffer = buf;
	++buf;
	--sz;
	if (sz < 0)
		obuffer = NULL;

	for (i=0;i<8;i++) {
		if (src[i] != 0) {
			notzero++;
			header |= 1<<i;
			if (sz > 0) {
				*buf = src[i];
				++buf;
				--sz;
			}
		}
	}
	if ((notzero == 7 || notzero == 6) && n > 0) {
		notzero = 8;
	}
	if (notzero == 8) {
		if (n > 0) {
			return 8;
		} else {
			return 10;
		}
	}
	if (obuffer) {
		*obuffer = header;
	}
	return notzero + 1;
}

static inline void
write_ff(const char * src, char * des, int n) {
	int i;
	int align8_n = (n+7)&(~7);

	des[0] = 0xff;
	des[1] = align8_n/8 - 1;
	memcpy(des+2, src, n);
	for(i=0; i< align8_n-n; i++){
		des[n+2+i] = 0;
	}
}

int
zproto_pack(const void * srcv, int srcsz, void * bufferv, int bufsz) {
	char tmp[8];
	int i;
	const char * ff_srcstart = NULL;
	char * ff_desstart = NULL;
	int ff_n = 0;
	int size = 0;
	const char * src = srcv;
	char * buffer = bufferv;
	for (i=0;i<srcsz;i+=8) {
		int n;
		int padding = i+8 - srcsz;
		if (padding > 0) {
			int j;
			memcpy(tmp, src, 8-padding);
			for (j=0;j<padding;j++) {
				tmp[7-j] = 0;
			}
			src = tmp;
		}
		n = pack_seg(src, buffer, bufsz, ff_n);
		bufsz -= n;
		if (n == 10) {
			// first FF
			ff_srcstart = src;
			ff_desstart = buffer;
			ff_n = 1;
		} else if (n==8 && ff_n>0) {
			++ff_n;
			if (ff_n == 256) {
				if (bufsz >= 0) {
					write_ff(ff_srcstart, ff_desstart, 256*8);
				}
				ff_n = 0;
			}
		} else {
			if (ff_n > 0) {
				if (bufsz >= 0) {
					write_ff(ff_srcstart, ff_desstart, ff_n*8);
				}
				ff_n = 0;
			}
		}
		src += 8;
		buffer += n;
		size += n;
	}
	if(bufsz >= 0){
		if(ff_n == 1)
			write_ff(ff_srcstart, ff_desstart, 8);
		else if (ff_n > 1)
			write_ff(ff_srcstart, ff_desstart, srcsz - (intptr_t)(ff_srcstart - (const char*)srcv));
	}
	return size;
}

int
zproto_unpack(const void * srcv, int srcsz, void * bufferv, int bufsz) {
	const char * src = srcv;
	char * buffer = bufferv;
	int size = 0;
	while (srcsz > 0) {
		char header = src[0];
		--srcsz;
		++src;
		if (header == 0xff) {
			int n;
			if (srcsz < 0) {
				return -1;
			}
			n = (src[0] + 1) * 8;
			if (srcsz < n + 1)
				return -1;
			srcsz -= n + 1;
			++src;
			if (bufsz >= n) {
				memcpy(buffer, src, n);
			}
			bufsz -= n;
			buffer += n;
			src += n;
			size += n;
		} else {
			int i;
			for (i=0;i<8;i++) {
				int nz = (header >> i) & 1;
				if (nz) {
					if (srcsz < 0)
						return -1;
					if (bufsz > 0) {
						*buffer = *src;
						--bufsz;
						++buffer;
					}
					++src;
					--srcsz;
				} else {
					if (bufsz > 0) {
						*buffer = 0;
						--bufsz;
						++buffer;
					}
				}
				++size;
			}
		}
	}
	return size;
}

