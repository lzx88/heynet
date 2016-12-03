#ifndef types_h
#define types_h

#ifdef _MSC_VER
# define inline __inline
# ifndef _MSC_STDINT_H_
#  if (_MSC_VER < 1300)
typedef signed char			int8;
typedef signed short		int16;
typedef signed int			int32;
typedef unsigned char		uint8;
typedef unsigned short		uint16;
typedef unsigned int		uint32;
#  else
typedef signed __int8		int8;
typedef signed __int16		int16;
typedef signed __int32		int32;
typedef unsigned __int8		uint8;
typedef unsigned __int16	uint16;
typedef unsigned __int32	uint32;
#  endif
typedef signed __int64      int64;
typedef unsigned __int64    uint64;
# endif
#else
#include <stdint.h>
typedef signed char			int8;
typedef short				int16;
typedef int					int32;
typedef long long			int64;
typedef unsigned char		uint8;
typedef unsigned short		uint16;
typedef unsigned int		uint32;
typedef unsigned long long	uint64;
#endif

// Min/Max type values
#define MIN_INT8 			(-127i8 - 1)
#define MIN_INT16			(-32767i16 - 1)
#define MIN_INT32			(-2147483647i32 - 1)
#define MIN_INT64			(-9223372036854775807i64 - 1)
#define MIN_INT128			(-170141183460469231731687303715884105727i128 - 1)

#define MAX_INT8			127i8
#define MAX_INT16			32767i16
#define MAX_INT32			2147483647i32
#define MAX_INT64			9223372036854775807i64
#define MAX_INT128			170141183460469231731687303715884105727i128
#define MAX_UINT8			0xffui8
#define MAX_UINT16			0xffffui16
#define MAX_UINT32			0xffffffffui32
#define MAX_UINT64			0xffffffffffffffffui64
#define MAX_UINT128			0xffffffffffffffffffffffffffffffffui128

#endif