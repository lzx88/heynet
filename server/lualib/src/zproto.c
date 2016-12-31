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

#define CHUNK_SIZE 1024

#define ZPROTO_TARRAY 0x80
#define SIZEOF_LENGTH 4
#define SIZEOF_HEADER 2
#define SIZEOF_FIELD 2

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

static inline int
toword(const uint8 *p) {
	return p[0] | p[1]<<8;
}

static inline uint32
todword(const uint8 *p) {
	return p[0] | p[1]<<8 | p[2]<<16 | p[3]<<24;
}

static int
count_array(const uint8 *stream) {
	uint32 length = todword(stream);
	int n = 0;
	stream += SIZEOF_LENGTH;
	while (length > 0) {
		uint32 nsz;
		if (length < SIZEOF_LENGTH)
			return -1;
		nsz = todword(stream);
		nsz += SIZEOF_LENGTH;
		if (nsz > length)
			return -1;
		++n;
		stream += nsz;
		length -= nsz;
	}

	return n;
}

static int
struct_field(const uint8 *stream, size_t sz) {
	const uint8 *field;
	int fn, header, i;
	if (sz < SIZEOF_LENGTH)
		return -1;
	fn = toword(stream);
	header = SIZEOF_HEADER + SIZEOF_FIELD *fn;
	if (sz < header)
		return -1;
	field = stream + SIZEOF_HEADER;
	sz -= header;
	stream += header;
	for (i=0;i<fn;i++) {
		int value= toword(field + i *SIZEOF_FIELD);
		uint32 dsz;
		if (value != 0)
			continue;
		if (sz < SIZEOF_LENGTH)
			return -1;
		dsz = todword(stream);
		if (sz < SIZEOF_LENGTH + dsz)
			return -1;
		stream += SIZEOF_LENGTH + dsz;
		sz -= SIZEOF_LENGTH + dsz;
	}

	return fn;
}

static const char *
import_string(struct zproto *s, const uint8 *stream) {
	uint32 sz = todword(stream);
	char *buffer = pool_alloc(&s->mem, sz+1);
	memcpy(buffer, stream+SIZEOF_LENGTH, sz);
	buffer[sz] = '\0';
	return buffer;
}

static const uint8 *
import_field(struct zproto *s, struct field *f, const uint8 *stream) {
	uint32 sz;
	const uint8 *result;
	int fn;
	int i;
	int array = 0;
	int tag = -1;
	f->tag = -1;
	f->type = -1;
	f->name = NULL;
	f->st = NULL;
	f->key = -1;

	sz = todword(stream);
	stream += SIZEOF_LENGTH;
	result = stream + sz;
	fn = struct_field(stream, sz);
	if (fn < 0)
		return NULL;
	stream += SIZEOF_HEADER;
	for (i=0;i<fn;i++) {
		int value;
		++tag;
		value = toword(stream + SIZEOF_FIELD *i);
		if (value & 1) {
			tag+= value/2;
			continue;
		}
		if (tag == 0) { // name
			if (value != 0)
				return NULL;
			f->name = import_string(s, stream + fn *SIZEOF_FIELD);
			continue;
		}
		if (value == 0)
			return NULL;
		value = value/2 - 1;
		switch(tag) {
		case 1: // buildin
			if (value >= ZT_TYPE)
				return NULL;	// invalid buildin type
			f->type = value;
			break;
		case 2: // type index
			if (value >= s->type_n)
				return NULL;	// invalid type index
			if (f->type >= 0)
				return NULL;
			f->type = ZT_TYPE;
			f->st = &s->type[value];
			break;
		case 3: // tag
			f->tag = value;
			break;
		case 4: // array
			if (value)
				array = ZPROTO_TARRAY;
			break;
		case 5:	// key
			f->key = value;
			break;
		default:
			return NULL;
		}
	}
	if (f->tag < 0 || f->type < 0 || f->name == NULL)
		return NULL;
	f->type |= array;

	return result;
}

/*
.type {
	.field {
		name 0 : string
		buildin 1 : integer
		type 2 : integer
		tag 3 : integer
		array 4 : boolean
	}
	name 0 : string
	fields 1 : *field
}
*/
static const uint8 *
import_type(struct zproto *s, struct type *t, const uint8 *stream) {
	const uint8 *result;
	uint32 sz = todword(stream);
	int i;
	int fn;
	int n;
	int maxn;
	int last;
	stream += SIZEOF_LENGTH;
	result = stream + sz;
	fn = struct_field(stream, sz);
	if (fn <= 0 || fn > 2)
		return NULL;
	for (i=0;i<fn*SIZEOF_FIELD;i+=SIZEOF_FIELD) {
		// name and fields must encode to 0
		int v = toword(stream + SIZEOF_HEADER + i);
		if (v != 0)
			return NULL;
	}
	memset(t, 0, sizeof(*t));
	stream += SIZEOF_HEADER + fn *SIZEOF_FIELD;
	t->name = import_string(s, stream);
	if (fn == 1) {
		return result;
	}
	stream += todword(stream)+SIZEOF_LENGTH;	// second data
	n = count_array(stream);
	if (n<0)
		return NULL;
	stream += SIZEOF_LENGTH;
	maxn = n;
	last = -1;
	t->n = n;
	t->f = pool_alloc(&s->mem, sizeof(struct field) *n);
	for (i=0;i<n;i++) {
		int tag;
		struct field *f = &t->f[i];
		stream = import_field(s, f, stream);
		if (stream == NULL)
			return NULL;
		tag = f->tag;
		if (tag <= last)
			return NULL;	// tag must in ascending order
		if (tag > last+1) {
			++maxn;
		}
		last = tag;
	}
	t->nf = maxn;
	t->base = t->f[0].tag;
	n = t->f[n-1].tag - t->base + 1;
	if (n != t->n) {
		t->base = -1;
	}
	return result;
}

/*
.protocol {
	name 0 : string
	tag 1 : integer
	request 2 : integer
	response 3 : integer
}
*/
static const uint8 *
import_protocol(struct zproto *s, struct protocol *p, const uint8 *stream) {
	const uint8 *result;
	uint32 sz = todword(stream);
	int fn;
	int i;
	int tag;
	stream += SIZEOF_LENGTH;
	result = stream + sz;
	fn = struct_field(stream, sz);
	stream += SIZEOF_HEADER;
	p->name = NULL;
	p->tag = -1;
	p->p[ZPROTO_REQUEST] = NULL;
	p->p[ZPROTO_RESPONSE] = NULL;
	tag = 0;
	for (i=0;i<fn;i++,tag++) {
		int value = toword(stream + SIZEOF_FIELD *i);
		if (value & 1) {
			tag += (value-1)/2;
			continue;
		}
		value = value/2 - 1;
		switch (i) {
		case 0: // name
			if (value != -1) {
				return NULL;
			}
			p->name = import_string(s, stream + SIZEOF_FIELD *fn);
			break;
		case 1: // tag
			if (value < 0) {
				return NULL;
			}
			p->tag = value;
			break;
		case 2: // request
			if (value < 0 || value>=s->type_n)
				return NULL;
			p->p[ZPROTO_REQUEST] = &s->type[value];
			break;
		case 3: // response
			if (value < 0 || value>=s->type_n)
				return NULL;
			p->p[ZPROTO_RESPONSE] = &s->type[value];
			break;
		default:
			return NULL;
		}
	}

	if (p->name == NULL || p->tag<0) {
		return NULL;
	}

	return result;
}

static struct zproto *
create_from_bundle(struct zproto *s, const uint8 *stream, size_t sz) {
	const uint8 *content;
	const uint8 *typedata = NULL;
	const uint8 *protocoldata = NULL;
	int fn = struct_field(stream, sz);
	int i;
	if (fn < 0 || fn > 2)
		return NULL;

	stream += SIZEOF_HEADER;
	content = stream + fn*SIZEOF_FIELD;

	for (i=0;i<fn;i++) {
		int value = toword(stream + i*SIZEOF_FIELD);
		int n;
		if (value != 0)
			return NULL;
		n = count_array(content);
		if (n<0)
			return NULL;
		if (i == 0) {
			typedata = content+SIZEOF_LENGTH;
			s->type_n = n;
			s->type = pool_alloc(&s->mem, n *sizeof(*s->type));
		} else {
			protocoldata = content+SIZEOF_LENGTH;
			s->np = n;
			s->p = pool_alloc(&s->mem, n *sizeof(*s->p));
		}
		content += todword(content) + SIZEOF_LENGTH;
	}

	for (i=0;i<s->type_n;i++) {
		typedata = import_type(s, &s->type[i], typedata);
		if (typedata == NULL) {
			return NULL;
		}
	}
	for (i=0;i<s->np;i++) {
		protocoldata = import_protocol(s, &s->p[i], protocoldata);
		if (protocoldata == NULL) {
			return NULL;
		}
	}

	return s;
}

struct zproto *
zproto_create(const void *proto, size_t sz) {
	struct memery mem;
	struct zproto *s;
	pool_init(&mem);
	s = pool_alloc(&mem, sizeof(*s));
	if (s == NULL)
		return NULL;
	memset(s, 0, sizeof(*s));
	s->mem = mem;
	if (create_from_bundle(s, proto, sz) == NULL) {
		pool_free(&s->mem);
		return NULL;
	}
	return s;
}

void
zproto_free(struct zproto *s) {
	if (s == NULL)
		return;
	pool_free(&s->mem);
}

void
zproto_dump(struct zproto *s) {
	int i,j;
	static const char *buildin[] = {
		"integer",
		"boolean",
		"string",
	};
	printf("=== %d types ===\n", s->type_n);
	for (i=0;i<s->type_n;i++) {
		struct type *t = &s->type[i];
		printf("%s\n", t->name);
		for (j=0;j<t->n;j++) {
			char array[2] = { 0, 0 };
			const char *type_name = NULL;
			struct field *f = &t->f[j];
			if (f->type & ZPROTO_TARRAY) {
				array[0] = '*';
			} else {
				array[0] = 0;
			}
			{
				int t = f->type & ~ZPROTO_TARRAY;
				if (t == ZT_TYPE) {
					type_name = f->st->name;
				} else {
					assert(t<ZT_TYPE);
					type_name = buildin[t];
				}
			}
			if (f->key >= 0) {
				printf("\t%s (%d) %s%s(%d)\n", f->name, f->tag, array, type_name, f->key);
			} else {
				printf("\t%s (%d) %s%s\n", f->name, f->tag, array, type_name);
			}
		}
	}
	printf("=== %d protocol ===\n", s->np);
	for (i=0;i<s->np;i++) {
		struct protocol *p = &s->p[i];
		if (p->p[ZPROTO_REQUEST]) {
			printf("\t%s (%d) request:%s", p->name, p->tag, p->p[ZPROTO_REQUEST]->name);
		} else {
			printf("\t%s (%d) request:(null)", p->name, p->tag);
		}
		if (p->p[ZPROTO_RESPONSE]) {
			printf(" response:%s", p->p[ZPROTO_RESPONSE]->name);
		}
		printf("\n");
	}
}

// query
int
zproto_prototag(const struct zproto *thiz, const char *name) {
	int i;
	for (i=0;i<thiz->np;i++) {
		if (strcmp(name, thiz->p[i].name) == 0) {
			return thiz->p[i].tag;
		}
	}
	return -1;
}

static struct protocol *
query_proto(const struct zproto *sp, int tag) {
	int begin = 0, end = sp->np;
	while(begin < end) {
		int mid = (begin + end) / 2;
		int t = sp->p[mid].tag;
		if (t==tag) {
			return &sp->p[mid];
		}
		if (tag > t) {
			begin = mid + 1;
		} else {
			end = mid;
		}
	}
	return NULL;
}

struct type *
zproto_protoquery(const struct zproto *sp, int proto, int what) {
	struct protocol *p;
	if (what <0 || what >1) {
		return NULL;
	}
	p = query_proto(sp, proto);
	if (p) {
		return p->p[what];
	}
	return NULL;
}

const char *
zproto_protoname(const struct zproto *sp, int proto) {
	struct protocol *p = query_proto(sp, proto);
	if (p) {
		return p->name;
	}
	return NULL;
}

struct type *
type(const struct zproto *sp, const char *type_name) {
	int i;
	for (i=0;i<sp->type_n;i++) {
		if (strcmp(type_name, sp->type[i].name) == 0) {
			return &sp->type[i];
		}
	}
	return NULL;
}

const char *
zproto_name(struct type *st) {
	return st->name;
}

static struct field *
findtag(const struct type *st, int tag) {
	int begin, end;
	if (st->base >=0 ) {
		tag -= st->base;
		if (tag < 0 || tag >= st->n)
			return NULL;
		return &st->f[tag];
	}
	begin = 0;
	end = st->n;
	while (begin < end) {
		int mid = (begin+end)/2;
		struct field *f = &st->f[mid];
		int t = f->tag;
		if (t == tag) {
			return f;
		}
		if (tag > t) {
			begin = mid + 1;
		} else {
			end = mid;
		}
	}
	return NULL;
}

// encode & decode
// zproto_callback(void *ud, int tag, int type, struct zproto_type *, void *value, int length)
//	  return size, -1 means error

static inline int
fill_size(uint8 *data, int sz) {
	data[0] = sz & 0xff;
	data[1] = (sz >> 8) & 0xff;
	data[2] = (sz >> 16) & 0xff;
	data[3] = (sz >> 24) & 0xff;
	return sz + SIZEOF_LENGTH;
}

static int
encode_integer(uint32 v, uint8 *data, int size) {
	if (size < SIZEOF_LENGTH + sizeof(v))
		return -1;
	data[4] = v & 0xff;
	data[5] = (v >> 8) & 0xff;
	data[6] = (v >> 16) & 0xff;
	data[7] = (v >> 24) & 0xff;
	return fill_size(data, sizeof(v));
}

static int
encode_uint64(uint64 v, uint8 *data, int size) {
	if (size < SIZEOF_LENGTH + sizeof(v))
		return -1;
	data[4] = v & 0xff;
	data[5] = (v >> 8) & 0xff;
	data[6] = (v >> 16) & 0xff;
	data[7] = (v >> 24) & 0xff;
	data[8] = (v >> 32) & 0xff;
	data[9] = (v >> 40) & 0xff;
	data[10] = (v >> 48) & 0xff;
	data[11] = (v >> 56) & 0xff;
	return fill_size(data, sizeof(v));
}

/*
//#define CB(tagname,type,index,subtype,value,length) cb(ud, tagname,type,index,subtype,value,length)

static int
do_cb(zproto_callback cb, void *ud, const char *tagname, int type, int index, struct zproto_type *subtype, void *value, int length) {
	if (subtype) {
		if (type >= 0) {
			printf("callback: tag=%s[%d], subtype[%s]:%d\n",tagname,index, subtype->name, type);
		} else {
			printf("callback: tag=%s[%d], subtype[%s]\n",tagname,index, subtype->name);
		}
	} else if (index > 0) {
		printf("callback: tag=%s[%d]\n",tagname,index);
	} else if (index == 0) {
		printf("callback: tag=%s\n",tagname);
	} else {
		printf("callback: tag=%s [mainkey]\n",tagname);
	}
	return cb(ud, tagname,type,index,subtype,value,length);
}
#define CB(tagname,type,index,subtype,value,length) do_cb(cb,ud, tagname,type,index,subtype,value,length)
*/

static int
encode_object(zproto_callback cb, struct zproto_arg *args, uint8 *data, int size) {
	int sz;
	if (size < SIZEOF_LENGTH)
		return -1;
	args->value = data+SIZEOF_LENGTH;
	args->length = size-SIZEOF_LENGTH;
	sz = cb(args);
	if (sz < 0) {
		if (sz == ZPROTO_CB_NIL)
			return 0;
		return -1;	// sz == ZPROTO_CB_ERROR
	}
	assert(sz <= size-SIZEOF_LENGTH);	// verify buffer overflow
	return fill_size(data, sz);
}

static inline void
uint32o_uint64(int negative, uint8 *buffer) {
	if (negative) {
		buffer[4] = 0xff;
		buffer[5] = 0xff;
		buffer[6] = 0xff;
		buffer[7] = 0xff;
	} else {
		buffer[4] = 0;
		buffer[5] = 0;
		buffer[6] = 0;
		buffer[7] = 0;
	}
}

static uint8 *
encode_integer_array(zproto_callback cb, struct zproto_arg *args, uint8 *buffer, int size, int *noarray) {
	uint8 *header = buffer;
	int intlen;
	int index;
	if (size < 1)
		return NULL;
	buffer++;
	size--;
	intlen = sizeof(uint32);
	index = 1;
	*noarray = 0;

	for (;;) {
		int sz;
		union {
			uint64 u64;
			uint32 u32;
		} u;
		args->value = &u;
		args->length = sizeof(u);
		args->index = index;
		sz = cb(args);
		if (sz <= 0) {
			if (sz == ZPROTO_CB_NIL) // nil object, end of array
				break;
			if (sz == ZPROTO_CB_NOARRAY) {	// no array, don't encode it
				*noarray = 1;
				break;
			}
			return NULL;	// sz == ZPROTO_CB_ERROR
		}
		if (size < sizeof(uint64))
			return NULL;
		if (sz == sizeof(uint32)) {
			uint32 v = u.u32;
			buffer[0] = v & 0xff;
			buffer[1] = (v >> 8) & 0xff;
			buffer[2] = (v >> 16) & 0xff;
			buffer[3] = (v >> 24) & 0xff;

			if (intlen == sizeof(uint64)) {
				uint32o_uint64(v & 0x80000000, buffer);
			}
		} else {
			uint64 v;
			if (sz != sizeof(uint64))
				return NULL;
			if (intlen == sizeof(uint32)) {
				int i;
				// rearrange
				size -= (index-1) *sizeof(uint32);
				if (size < sizeof(uint64))
					return NULL;
				buffer += (index-1) *sizeof(uint32);
				for (i=index-2;i>=0;i--) {
					int negative;
					memcpy(header+1+i*sizeof(uint64), header+1+i*sizeof(uint32), sizeof(uint32));
					negative = header[1+i*sizeof(uint64)+3] & 0x80;
					uint32o_uint64(negative, header+1+i*sizeof(uint64));
				}
				intlen = sizeof(uint64);
			}

			v = u.u64;
			buffer[0] = v & 0xff;
			buffer[1] = (v >> 8) & 0xff;
			buffer[2] = (v >> 16) & 0xff;
			buffer[3] = (v >> 24) & 0xff;
			buffer[4] = (v >> 32) & 0xff;
			buffer[5] = (v >> 40) & 0xff;
			buffer[6] = (v >> 48) & 0xff;
			buffer[7] = (v >> 56) & 0xff;
		}

		size -= intlen;
		buffer += intlen;
		index++;
	}

	if (buffer == header + 1) {
		return header;
	}
	*header = (uint8)intlen;
	return buffer;
}

static int
encode_array(zproto_callback cb, struct zproto_arg *args, uint8 *data, int size) {
	uint8 *buffer;
	int sz;
	if (size < SIZEOF_LENGTH)
		return -1;
	size -= SIZEOF_LENGTH;
	buffer = data + SIZEOF_LENGTH;
	switch (args->type) {
	case ZT_NUMBER: {
		int noarray;
		buffer = encode_integer_array(cb,args,buffer,size, &noarray);
		if (buffer == NULL)
			return -1;
	
		if (noarray) {
			return 0;
		}
		break;
	}
	case ZT_BOOL:
		args->index = 1;
		for (;;) {
			int v = 0;
			args->value = &v;
			args->length = sizeof(v);
			sz = cb(args);
			if (sz < 0) {
				if (sz == ZPROTO_CB_NIL)		// nil object , end of array
					break;
				if (sz == ZPROTO_CB_NOARRAY)	// no array, don't encode it
					return 0;
				return -1;	// sz == ZPROTO_CB_ERROR
			}
			if (size < 1)
				return -1;
			buffer[0] = v ? 1: 0;
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
			args->value = buffer+SIZEOF_LENGTH;
			args->length = size;
			sz = cb(args);
			if (sz < 0) {
				if (sz == ZPROTO_CB_NIL) {
					break;
				}
				if (sz == ZPROTO_CB_NOARRAY)	// no array, don't encode it
					return 0;
				return -1;	// sz == ZPROTO_CB_ERROR
			}
			fill_size(buffer, sz);
			buffer += SIZEOF_LENGTH+sz;
			size -=sz;
			++args->index;
		}
		break;
	}
	sz = buffer - (data + SIZEOF_LENGTH);
	return fill_size(data, sz);
}

int
zproto_encode(const struct type *st, void *buffer, int size, zproto_callback cb, void *ud) {
	struct zproto_arg args;
	uint8 *header = buffer;
	uint8 *data;
	int header_sz = SIZEOF_HEADER + st->nf *SIZEOF_FIELD;
	int i;
	int index;
	int lasttag;
	int datasz;
	if (size < header_sz)
		return -1;
	args.ud = ud;
	data = header + header_sz;
	size -= header_sz;
	index = 0;
	lasttag = -1;
	for (i=0;i<st->nf;i++) {
		struct field *f = &st->f[i];
		int type = f->type;
		int value = 0;
		int sz = -1;
		args.tagname = f->name;
		args.tagid = f->tag;
		args.subtype = f->st;
		args.mainindex = f->key;
		if (type & ZPROTO_TARRAY) {
			args.type = type & ~ZPROTO_TARRAY;
			sz = encode_array(cb, &args, data, size);
		} else {
			args.type = type;
			args.index = 0;
			switch(type) {
			case ZT_NUMBER:
			case ZT_BOOL: {
				union {
					uint64 u64;
					uint32 u32;
				} u;
				args.value = &u;
				args.length = sizeof(u);
				sz = cb(&args);
				if (sz < 0) {
					if (sz == ZPROTO_CB_NIL)
						continue;
					if (sz == ZPROTO_CB_NOARRAY)	// no array, don't encode it
						return 0;
					return -1;	// sz == ZPROTO_CB_ERROR
				}
				if (sz == sizeof(uint32)) {
					if (u.u32 < 0x7fff) {
						value = (u.u32+1) *2;
						sz = 2; // sz can be any number > 0
					} else {
						sz = encode_integer(u.u32, data, size);
					}
				} else if (sz == sizeof(uint64)) {
					sz= encode_uint64(u.u64, data, size);
				} else {
					return -1;
				}
				break;
			}
			case ZT_TYPE:
			case ZT_STRING:
				sz = encode_object(cb, &args, data, size);
				break;
			}
		}
		if (sz < 0)
			return -1;
		if (sz > 0) {
			uint8 *record;
			int tag;
			if (value == 0) {
				data += sz;
				size -= sz;
			}
			record = header+SIZEOF_HEADER+SIZEOF_FIELD*index;
			tag = f->tag - lasttag - 1;
			if (tag > 0) {
				// skip tag
				tag = (tag - 1) *2 + 1;
				if (tag > 0xffff)
					return -1;
				record[0] = tag & 0xff;
				record[1] = (tag >> 8) & 0xff;
				++index;
				record += SIZEOF_FIELD;
			}
			++index;
			record[0] = value & 0xff;
			record[1] = (value >> 8) & 0xff;
			lasttag = f->tag;
		}
	}
	header[0] = index & 0xff;
	header[1] = (index >> 8) & 0xff;

	datasz = data - (header + header_sz);
	data = header + header_sz;
	if (index != st->nf) {
		memmove(header + SIZEOF_HEADER + index *SIZEOF_FIELD, data, datasz);
	}
	return SIZEOF_HEADER + index *SIZEOF_FIELD + datasz;
}

static int
decode_array_object(zproto_callback cb, struct zproto_arg *args, uint8 *stream, int sz) {
	uint32 hsz;
	int index = 1;
	while (sz > 0) {
		if (sz < SIZEOF_LENGTH)
			return -1;
		hsz = todword(stream);
		stream += SIZEOF_LENGTH;
		sz -= SIZEOF_LENGTH;
		if (hsz > sz)
			return -1;
		args->index = index;
		args->value = stream;
		args->length = hsz;
		if (cb(args))
			return -1;
		sz -= hsz;
		stream += hsz;
		++index;
	}
	return 0;
}

static inline uint64
expand64(uint32 v) {
	uint64 value = v;
	if (value & 0x80000000) {
		value |= (uint64)~0  << 32 ;
	}
	return value;
}

static int
decode_array(zproto_callback cb, struct zproto_arg *args, uint8 *stream) {
	uint32 sz = todword(stream);
	int type = args->type;
	int i;
	if (sz == 0) {
		// It's empty array, call cb with index == -1 to create the empty array.
		args->index = -1;
		args->value = NULL;
		args->length = 0;
		cb(args);
		return 0;
	}	
	stream += SIZEOF_LENGTH;
	switch (type) {
	case ZT_NUMBER: {
		int len = *stream;
		++stream;
		--sz;
		if (len == sizeof(uint32)) {
			if (sz % sizeof(uint32) != 0)
				return -1;
			for (i=0;i<sz/sizeof(uint32);i++) {
				uint64 value = expand64(todword(stream + i*sizeof(uint32)));
				args->index = i+1;
				args->value = &value;
				args->length = sizeof(value);
				cb(args);
			}
		} else if (len == sizeof(uint64)) {
			if (sz % sizeof(uint64) != 0)
				return -1;
			for (i=0;i<sz/sizeof(uint64);i++) {
				uint64 low = todword(stream + i*sizeof(uint64));
				uint64 hi = todword(stream + i*sizeof(uint64) + sizeof(uint32));
				uint64 value = low | hi << 32;
				args->index = i+1;
				args->value = &value;
				args->length = sizeof(value);
				cb(args);
			}
		} else {
			return -1;
		}
		break;
	}
	case ZT_BOOL:
		for (i=0;i<sz;i++) {
			uint64 value = stream[i];
			args->index = i+1;
			args->value = &value;
			args->length = sizeof(value);
			cb(args);
		}
		break;
	case ZT_STRING:
	case ZT_TYPE:
		return decode_array_object(cb, args, stream, sz);
	default:
		return -1;
	}
	return 0;
}

int
zproto_decode(const struct type *st, const void *data, int size, zproto_callback cb, void *ud) {
	struct zproto_arg args;
	int total = size;
	uint8 *stream;
	uint8 *datastream;
	int fn;
	int i;
	int tag;
	if (size < SIZEOF_HEADER)
		return -1;
	// debug print
	// printf("zproto_decode[%p] (%s)\n", ud, st->name);
	stream = (void *)data;
	fn = toword(stream);
	stream += SIZEOF_HEADER;
	size -= SIZEOF_HEADER ;
	if (size < fn *SIZEOF_FIELD)
		return -1;
	datastream = stream + fn *SIZEOF_FIELD;
	size -= fn *SIZEOF_FIELD;
	args.ud = ud;

	tag = -1;
	for (i=0;i<fn;i++) {
		uint8 *currentdata;
		struct field *f;
		int value = toword(stream + i *SIZEOF_FIELD);
		++ tag;
		if (value & 1) {
			tag += value/2;
			continue;
		}
		value = value/2 - 1;
		currentdata = datastream;
		if (value < 0) {
			uint32 sz;
			if (size < SIZEOF_LENGTH)
				return -1;
			sz = todword(datastream);
			if (size < sz + SIZEOF_LENGTH)
				return -1;
			datastream += sz+SIZEOF_LENGTH;
			size -= sz+SIZEOF_LENGTH;
		}
		f = findtag(st, tag);
		if (f == NULL)
			continue;
		args.tagname = f->name;
		args.tagid = f->tag;
		args.type = f->type & ~ZPROTO_TARRAY;
		args.subtype = f->st;
		args.index = 0;
		args.mainindex = f->key;
		if (value < 0) {
			if (f->type & ZPROTO_TARRAY) {
				if (decode_array(cb, &args, currentdata)) {
					return -1;
				}
			} else {
				switch (f->type) {
				case ZT_NUMBER: {
					uint32 sz = todword(currentdata);
					if (sz == sizeof(uint32)) {
						uint64 v = expand64(todword(currentdata + SIZEOF_LENGTH));
						args.value = &v;
						args.length = sizeof(v);
						cb(&args);
					} else if (sz != sizeof(uint64)) {
						return -1;
					} else {
						uint32 low = todword(currentdata + SIZEOF_LENGTH);
						uint32 hi = todword(currentdata + SIZEOF_LENGTH + sizeof(uint32));
						uint64 v = (uint64)low | (uint64) hi << 32;
						args.value = &v;
						args.length = sizeof(v);
						cb(&args);
					}
					break;
				}
				case ZT_STRING:
				case ZT_TYPE: {
					uint32 sz = todword(currentdata);
					args.value = currentdata+SIZEOF_LENGTH;
					args.length = sz;
					if (cb(&args))
						return -1;
					break;
				}
				default:
					return -1;
				}
			}
		} else if (f->type != ZT_NUMBER && f->type != ZT_BOOL) {
			return -1;
		} else {
			uint64 v = value;
			args.value = &v;
			args.length = sizeof(v);
			cb(&args);
		}
	}
	return total - size;
}

// 0 pack

static int
pack_seg(const uint8 *src, uint8 *buffer, int sz, int n) {
	uint8 header = 0;
	int notzero = 0;
	int i;
	uint8 *obuffer = buffer;
	++buffer;
	--sz;
	if (sz < 0)
		obuffer = NULL;

	for (i=0;i<8;i++) {
		if (src[i] != 0) {
			notzero++;
			header |= 1<<i;
			if (sz > 0) {
				*buffer = src[i];
				++buffer;
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
write_ff(const uint8 *src, uint8 *des, int n) {
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
zproto_pack(const void *srcv, int srcsz, void *bufferv, int bufsz) {
	uint8 tmp[8];
	int i;
	const uint8 *ff_srcstart = NULL;
	uint8 *ff_desstart = NULL;
	int ff_n = 0;
	int size = 0;
	const uint8 *src = srcv;
	uint8 *buffer = bufferv;
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
			write_ff(ff_srcstart, ff_desstart, srcsz - (intptr_t)(ff_srcstart - (const uint8*)srcv));
	}
	return size;
}

int
zproto_unpack(const void *srcv, int srcsz, void *bufferv, int bufsz) {
	const uint8 *src = srcv;
	uint8 *buffer = bufferv;
	int size = 0;
	while (srcsz > 0) {
		uint8 header = src[0];
		--srcsz;
		++src;
		if (header == 0xff) {
			int n;
			if (srcsz < 0) {
				return -1;
			}
			n = (src[0] + 1) *8;
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
