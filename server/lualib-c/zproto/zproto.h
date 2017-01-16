#ifndef zproto_h
#define zproto_h
#include <stddef.h>

#define ZT_BOOL	  -3
#define ZT_STRING -2
#define ZT_INTEGER -1
// >=0 �Զ������� type tag
#define ZK_ARRAY  0
#define ZK_NULL  -1
// ZK_MAP>0 �Զ������� field tag

struct zproto;
struct zproto_type;
struct zproto_field;
struct zproto_encode_arg {
	struct zproto_field *field;
	void *result;
	int length;
	void *ud;
};

#define ZPROTO_CB_ERROR -1
#define ZPROTO_CB_NIL	-2
#define ZPROTO_CB_NOARRAY -3

typedef int(*zproto_cb)(const struct zproto_encode_arg *args);
#endif
