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

void zproto_init(struct zproto *thiz);
void *zproto_alloc(struct zproto *thiz, size_t sz);
struct zproto *zproto_done(struct zproto *thiz);
void zproto_free(struct zproto *thiz);

const struct type *zproto_import(const struct zproto *zp, int idx);
const struct protocol *zproto_query(struct zproto *thiz, const char *tyname);

#define ZPROTO_CB_MEM		-1
#define ZPROTO_CB_ERR		-2
#define ZPROTO_CB_NIL		-3

typedef int(*zproto_cb)(struct zproto_arg *args);
int zproto_encode(const struct type *ty, void *buffer, int size, zproto_cb cb, void *ud);
int zproto_decode(const struct type *ty, const char *data, int size, bool shift, zproto_cb cb, void *ud);
int zproto_pack(const char* input, int ilen, char *output, int olen);
int zproto_unpack(const char *input, int lien, char *output, int olen);
#endif
