#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include "types.h"

#include "zproto.h"

#define ZT_NUMBER -1
#define ZT_STRING -2
#define ZT_BOOL	  -3
// >0 自定义类型 tag

#define CHUNK_SIZE 1024
#define SIZEOF_LENGTH 2

typedef struct {
	int curr;
	int size;
	void *ptr;
}memery;
typedef struct {
	int tag;
	int type;
	int key;
	const char name[];
}field;
typedef struct {
	int nf;
	field *f;
	const char name[];
}type, zproto_type;
typedef struct {
	int tag;
	int response;
	int nf;
	field *f;
	const char name[];
}protocol;
typedef struct {
	memery mem;
	int np;
	protocol *p;
	int tn;
	type *t;
}zproto;

static void
pool_init(memery *m) {
	m->size = 0;
	m->ptr = NULL;
	m->curr = 0;
}
static void
pool_free(struct memery *m) {
	if (m->ptr)
		free(m->ptr);
	m->size = 0;
	m->ptr = NULL;
	m->curr = 0;
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
static int
pool_alloc(struct memery *m, size_t sz) {
	sz = (sz + 3) & ~3;	// align by 4
	void *result = NULL;
	if (m->ptr == NULL) {
		m->size = sz > CHUNK_SIZE ? sz : CHUNK_SIZE;
		m->ptr = malloc(m->size);
		result = m->ptr;
	}
	else if (sz > CHUNK_SIZE)
		result = pool_enlarge(m, m->size + sz);
	else if (sz + m->curr > m->size)
		result = pool_enlarge(m, m->size + CHUNK_SIZE);
	else
		result = m->ptr + m->curr;
	int pos = -1;
	if (result)
	{
		pos = m->curr;
		m->curr += sz;
	}
	return pos;
}
static void*
pool_compact(struct memery *m){
	return pool_enlarge(m, m->curr)
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
	const char bendian;
	uint16 nbytes;
	uint16 msgid;
	uint32 session;
};
#pragma pack()

typedef int64 number;

struct zstruct{
	uint16 n;//数据个数
	uint16* tags;//n个字段的标签
	uint32* nums;//n个字段的数据
	char* data;//数据块 头两字节为长度
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
	uint32 n = 0;
	if (i < 0)
		n = (-i) << 1;
	else(i > 0)
		n = i << 1 + 1;
	write_buf(buf, &v, sizeof(n));
	return true;
}

void write_string(char* buf, char* cstr){
	uint16 len = strlen(cstr);
	write_buf(buf, &len, sizeof(len));
	write_buf(buf, cstr, len + 1);
}


void write_header(header* head, char* buf){
	write_buf(buf, head, sizeof(*head));
}

void write_struct(zstruct* content, char* buf){
	write_buf(buf, content->n, sizeof(content->n));
	write_tag(buf + 2, content->n, sizeof(content->n));

}

void write_message(message* msg, char* buf, size_t s){
	write_header(msg->head, buf);
	write_struct(msg->content, buf);
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

