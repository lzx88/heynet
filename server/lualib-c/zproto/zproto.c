#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include "msvcint.h"

#include "zproto.h"

#define ZT_NUMBER -1
#define ZT_STRING -2
#define ZT_BOOL	  -3
// >0 自定义类型 tag

#define CHUNK_SIZE 0x100000 //1M
#define LEN_SIZE 4

struct memery{
	int curr;
	int size;
	void *ptr;
};
struct field{
	const char *name;
	int tag;
	int type;
	int key;
};
struct type{
	const char *name;
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

static struct zproto *
zproto_alloc(){
	struct memery m;
	pool_init(&m);
	zproto* thiz = pool_alloc(&m, sizeof(*thiz));
	thiz->mem = m;
	thiz->pn = 0;
	thiz->p = NULL;
	thiz->tn = 0;
	thiz->t = NULL;
	return thiz;
}
static bool
zproto_done(struct zproto *thiz){
	void *base = thiz;
	thiz = pool_enlarge(&thiz->mem, thiz->mem.curr);
	if (thiz == NULL)
		return false;
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
		if (tp->p[SPROTO_RESPONSE]) {
			printf(" response:%s", tp->p[SPROTO_RESPONSE]->name);
		}
		printf("\n");
	}
}


static struct type *
zproto_import(struct zproto *thiz, int tidx) {
	return 0 <= tidx && tidx < thiz->tn ? thiz->t[tidx] : NULL;
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

static struct protocol *
zproto_query(struct zproto *thiz, const char* name) {
	for (int i = 0; i < thiz->pn; i++) {
		struct type* request = zproto_import(thiz, thiz->p[i]->request);
		if (0 == strcmp(name, request->name)) {
			return thiz->p[i];
		}
	}
	return NULL;
}

//encode/decode
struct header;
struct zstruct;

struct message{
	header head;
	zstruct content;
};

static char test_endian(){
	union {
		char c;
		short s;
	}u;
	u.s = "BL";
	return c;
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

typedef int64 number;

struct zstruct{
	uint16 n;//数据个数
	uint16* tags;//n个字段的标签
	uint32* nums;//n个字段的数据
	char data[];//数据块 头4字节为长度
};

void write_buf(char* buf, char* data, size_t len){
	memcpy(buf, data, len);
}

void write_tag(char* buf, uint16 tag, uint16 pre){
	tag -= pre + 1;
	write_buf(buf, &tag, sizeof(tag));
}

bool write_number(char* buf, number i){
	if ((i & ~0x7FFFFFFF) > 0)
		return false;
	uint32 n = i >= 0 ? (i << 1 + 1) : ((-i) << 1);
	write_buf(buf, &n, sizeof(n));
	return true;
}

void write_string(char* buf, char* cstr){
	uint16 len = strlen(cstr);
	write_buf(buf, &len, sizeof(len));
	write_buf(buf, cstr, len + 1);
}

void write_struct(zstruct* content, char* buf){
	write_buf(buf, content->n, sizeof(content->n));
	write_tag(buf + 2, content->n, sizeof(content->n));

}

void write_message(char* buf, size_t s, message* msg){
	write_buf(buf, msg->head, sizeof(msg->head));
	write_struct(buf, msg->content);
}

uint16 read_tag(char* buf){
	return *buf;
}

number read_number(char* buf){
	uint32 n = 0;
	memcpy(&n, buf, sizeof(n));
	if (n & 1)
		return n >> 1;
	return -(n >> 1);
}

