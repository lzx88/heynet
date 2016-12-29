#ifndef zproto_h
#define zproto_h

#include <stddef.h>

struct zproto;
struct type;

#define ZPROTO_REQUEST 0
#define ZPROTO_RESPONSE 1



#define ZPROTO_CB_ERROR -1
#define ZPROTO_CB_NIL -2
#define ZPROTO_CB_NOARRAY -3

struct zproto *zproto_create(const void *proto, size_t sz);
void zproto_free(struct zproto *);

int zproto_prototag(const struct zproto *, const char *name);
const char *zproto_protoname(const struct zproto *, int proto);
// ZPROTO_REQUEST(0) : request, ZPROTO_RESPONSE(1): response
struct zproto_type *zproto_protoquery(const struct zproto *, int proto, int what);

struct zproto_type *zproto_type(const struct zproto *, const char *type_name);

int zproto_pack(const void *src, int srcsz, void *buffer, int bufsz);
int zproto_unpack(const void *src, int srcsz, void *buffer, int bufsz);

struct zproto_arg {
	void *ud;
	const char *tagname;
	int tagid;
	int type;
	struct type *subtype;
	void *value;
	int length;
	int index;	// array base 1
	int mainindex;	// for map
};

typedef int (*zproto_callback)(const struct zproto_arg *args);

int zproto_decode(const zproto_type *, const void *data, int size, zproto_callback cb, void *ud);
int zproto_encode(const zproto_type *, void *buffer, int size, zproto_callback cb, void *ud);

// for debug use
void zproto_dump(struct zproto *);
const char *zproto_name(struct zproto_type *);

#endif
