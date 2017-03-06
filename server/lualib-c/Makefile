.PHONY :linux win32 clean

LUA_CLIB_PATH=.
ZPROTO_PATH=./zproto

CC=gcc -std=gnu99 -O2 
CFLAGS=-Wall -Wextra $(SYSCFLAGS) $(MYCFLAGS)
CDEBUG=-g -O0
SYSCFLAGS=-Wno-int-to-pointer-cast
MYCFLAGS=-I../skynet/3rd/lua

# For Linux
linux:
	make ../cservice/zproto.so "DLLFLAGS = -shared -fPIC"
# For Mac OS
macosx:
	make ../cservice/zproto.so "DLLFLAGS = -bundle -undefined dynamic_lookup"
# For win32
win32: zproto.dll

../cservice/zproto.so : $(ZPROTO_PATH)/zproto.c $(ZPROTO_PATH)/zproto_lua.c
	env $(CC) $(CDEBUG) $(CFLAGS) $(DLLFLAGS) -o $@ $^

zproto.dll : zproto.c zproto_lua.c
	$(CC) -Wall --shared -o $@ $^ -I/usr/local/include -L/usr/local/bin -llua53

clean :
	rm -f ../cservice/zproto.so zproto.dll
