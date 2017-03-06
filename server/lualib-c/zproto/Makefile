.PHONY :linux win32 clean

TARGET=../zproto.so

CC=gcc -std=gnu99 -O2 
CFLAGS=-Wall -Wextra $(SYSCFLAGS) $(MYCFLAGS)
CDEBUG=-g -O0
SYSCFLAGS=-Wno-int-to-pointer-cast
MYCFLAGS=-I../../skynet/3rd/lua

# For Linux
linux:
	make $(TARGET) "DLLFLAGS = -shared -fPIC"
# For Mac OS
macosx:
	make $(TARGET) "DLLFLAGS = -bundle -undefined dynamic_lookup"
# For win32
win32: zproto.dll

$(TARGET) : zproto.c zproto_lua.c
	env $(CC) $(CDEBUG) $(CFLAGS) $(DLLFLAGS) -o $@ $^

zproto.dll : zproto.c zproto_lua.c
	$(CC) -Wall --shared -o $@ $^ -I/usr/local/include -L/usr/local/bin -llua53

clean :
	rm -f $(TARGET) zproto.dll
