#ifndef msvc_int_h
#define msvc_int_h

#ifdef _MSC_VER
# define inline __inline
# ifndef _MSC_STDINT_H_
#  if (_MSC_VER < 1300)
typedef signed char       int8;
typedef signed short      int16;
typedef signed int        int32;
typedef unsigned char     uint8;
typedef unsigned short    uint16;
typedef unsigned int      uint32;
#  else
typedef signed __int8     int8;
typedef signed __int16    int16;
typedef signed __int32    int32;
typedef unsigned __int8   uint8;
typedef unsigned __int16  uint16;
typedef unsigned __int32  uint32;
#  endif
typedef signed __int64		int64;
typedef unsigned __int64	uint64;
# endif

#else

#include <stdint.h>
typedef int8_t		int8;
typedef int16_t		int16;
typedef int32_t     int32;
typedef int64_t     int64;
typedef uint8_t     uint8;
typedef uint16_t    uint16;
typedef uint32_t    uint32;
typedef uint64_t    uint64;

#endif

#ifndef bool
#define bool char
#define ture 1
#define false 0
#endif

#endif
