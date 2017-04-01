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

struct memery{
	size_t size;
	void *ptr;
};

struct zfield{
	const char *name;
	int tag;
	int type;
	int key;
};
struct ztype{
	const char *name;
	int maxn;
	int n;
	struct zfield *f;
};
struct zprotocol{
	int tag;
	int request;
	int response;
};
struct zproto{
	struct memery mem;
	char endian;
	int pn;
	struct zprotocol *p;
	int tn;
	struct ztype *t;
};
typedef int64 integer;
int16 shift16(const char* p, bool shift);
int32 shift32(const char* p, bool shift);
int64 shift64(const char* p, bool shift);

void zproto_init(struct zproto *thiz);
void *zproto_alloc(struct zproto *thiz, size_t sz);
struct zproto *zproto_done(struct zproto *thiz);
void zproto_free(struct zproto *thiz);

const struct ztype *zproto_import(const struct zproto *zp, int idx);
const struct zprotocol *zproto_findname(const struct zproto *thiz, const char *tyname);
const struct zprotocol *zproto_findtag(const struct zproto *thiz, int tag);

#define ZCB_MEM		-1
#define ZCB_ERR		-2
#define ZCB_NIL		-3
struct zproto_arg {
	const struct zfield *pf;
	void *ud;
	void *value;
	int length;
	int index;
	char shift;
};
int zproto_pack(const char* data, int size, char *buffer, int len);
int zproto_unpack(const char *data, int size, char *buffer, int len);
typedef int(*zproto_cb)(struct zproto_arg *args);
int zproto_encode(const struct ztype *ty, char *buffer, int size, zproto_cb cb, void *ud);
int zproto_decode(const struct ztype *ty, const char *data, int size, bool shift, zproto_cb cb, void *ud);
#endif
