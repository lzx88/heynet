SKYNET_PATH = skynet
TARGET = cservice/package.so

$(TARGET) : service-c/service_package.c
	gcc -Wall -O2 --shared -fPIC -o $@ $^ -I$(SKYNET_PATH)/skynet-src

socket :
	cd client-simple/lsocket && $(MAKE) LUA_INCLUDE=../../skynet/3rd/lua

clean :
	rm $(TARGET)
