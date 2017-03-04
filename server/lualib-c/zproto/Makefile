.PHONY : all win clean

all : linux
win : zproto.dll

# For Linux
linux:
	make zproto.so "DLLFLAGS = -shared -fPIC"
# For Mac OS
macosx:
	make zproto.so "DLLFLAGS = -bundle -undefined dynamic_lookup"

zproto.so : zproto.c zproto_lua.c
	env gcc -O2 -Wall $(DLLFLAGS) -o $@ $^

zproto.dll : zproto.c zproto_lua.c
	gcc -O2 -Wall --shared -o $@ $^ -I/usr/local/include -L/usr/local/bin -llua53

clean :
rm -f zproto.so zproto.dll
