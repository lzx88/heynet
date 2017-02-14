#ifndef zproto_h
#define zproto_h
#include <stddef.h>

#define ZT_BOOL	  -3
#define ZT_STRING -2
#define ZT_INTEGER -1
// >=0 自定义类型 type tag
#define ZK_NULL   0
#define ZK_ARRAY  1
#define ZK_MAP	  2

struct zproto;
typedef struct type zproto_type;
typedef struct field zproto_field;
struct zproto_arg {
	const zproto_field *pf;
	void *value;
	int length;
	void *ud;
	int index;
};

#define ZPROTO_CB_ERROR		-1
#define ZPROTO_CB_NIL		-2
#define ZPROTO_CB_NOARRAY	-3

int zproto_encode(const zproto_type *ty, void *buffer, int size, zproto_cb cb, void *ud);
typedef int(*zproto_cb)(const zproto_arg *args);
#endif
