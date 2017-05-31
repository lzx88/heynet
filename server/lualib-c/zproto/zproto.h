#ifndef zproto_h
#define zproto_h
#include <stddef.h>

#define ZT_BOOL	  -3
#define ZT_STRING -2
#define ZT_INTEGER -1
// >=0 自定义type tag
#define ZK_NULL   0
#define ZK_ARRAY  1
#define ZK_MAP	  2

typedef int64 integer;

struct memery{
	size_t size;
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
	char endian;
	int pn;
	struct protocol *p;
	int tn;
	struct type *t;
};

struct zproto_arg {
	const struct field *pf;
	void *ud;
	void *value;
	int length;
	int index;
	char shift;
};

#define _shift16(p) (int16)p[1] | (int16)p[0] << 8
#define _shift32(p) (int16)p[3] | (int16)p[2] << 8 | (int32)p[1] << 16 | (int32)p[0] << 24
#define _shift64(p) (int16)p[7] | (int16)p[6] << 8 | (int32)p[5] << 16 | (int32)p[4] << 24 | (int64)p[3] << 32 | (int64)p[2] << 40 | (int64)p[1] << 48 | (int64)p[0] << 56
static int16 shift16(const char* p, bool shift){ return shift ? _shift16(p) : *(int16*)p; }
static int32 shift32(const char* p, bool shift){ return shift ? _shift32(p) : *(int32*)p; }
static int64 shift64(const char* p, bool shift){ return shift ? _shift64(p) : *(int64*)p; }

void zproto_init(struct zproto *thiz);
void *zproto_alloc(struct zproto *thiz, size_t sz);
struct zproto *zproto_done(struct zproto *thiz);
void zproto_free(struct zproto *thiz);

const struct type *zproto_import(const struct zproto *zp, int idx);
const struct protocol *zproto_findname(struct zproto *thiz, const char *tyname);
const struct protocol *zproto_findtag(struct zproto *thiz, int tag);

#define ZPROTO_CB_MEM		-1
#define ZPROTO_CB_ERR		-2
#define ZPROTO_CB_NIL		-3

typedef int(*zproto_cb)(struct zproto_arg *args);
int zproto_encode(const struct type *ty, void *buffer, int size, zproto_cb cb, void *ud);
int zproto_decode(const struct type *ty, const char *data, int size, bool shift, zproto_cb cb, void *ud);
int zproto_pack(const char* input, int ilen, char *output, int olen);
int zproto_unpack(const char *input, int lien, char *output, int olen);
#endif
