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
	void *ud;
	void *value;
	int length;
	int index;
	char shift;
};

#define ZPROTO_CB_MEM		-1
#define ZPROTO_CB_ERR		-2
#define ZPROTO_CB_NIL		-3

typedef int(*zproto_cb)(const struct zproto_arg *args);
int zproto_encode(const zproto_type *ty, void *buffer, int size, zproto_cb cb, void *ud);
#endif
