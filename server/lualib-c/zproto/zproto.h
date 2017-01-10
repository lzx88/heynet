#ifndef zproto_h
#define zproto_h
#include <stddef.h>

struct zproto;
struct zproto_type;
struct zproto_field;
struct zproto_encode_arg {
	struct zproto_field *field;
	void *ud;
};

typedef int(*zproto_cb)(const struct zproto_encode_arg *args);
#endif
