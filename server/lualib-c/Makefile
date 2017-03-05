.PHONY :linux win32 clean

LUA_CLIB_PATH=.
ZPROTO_PATH=$(LUA_CLIB_PATH)/zproto

CC=gcc -std=gnu99 -g
CFLAGS=-O2 -Wall -Wextra $(SYSCFLAGS) $(MYCFLAGS)

SYSCFLAGS=-Wno-int-to-pointer-cast
MYCFLAGS=-I../skynet/3rd/lua

# For Linux
linux:
	make $(LUA_CLIB_PATH)/zproto.so "DLLFLAGS = -shared -fPIC"
# For Mac OS
macosx:
	make $(LUA_CLIB_PATH)/zproto.so "DLLFLAGS = -bundle -undefined dynamic_lookup"
# For win32
win32: zproto.dll

$(LUA_CLIB_PATH)/zproto.so : $(ZPROTO_PATH)/zproto.c $(ZPROTO_PATH)/zproto_lua.c
	env $(CC) $(CFLAGS) $(DLLFLAGS) -o $@ $^

zproto.dll : zproto.c zproto_lua.c
	$(CC) -Wall --shared -o $@ $^ -I/usr/local/include -L/usr/local/bin -llua53

clean :
	rm -f $(LUA_CLIB_PATH)zproto.so zproto.dll
