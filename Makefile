CFLAGS := -Wall --std=gnu99 -g3 -Wno-format-security -Werror -fsanitize=address -fno-omit-frame-pointer
CURL_LIBS := $(shell curl-config --libs)
CURL_CFLAGS := $(shell curl-config --cflags)

ARCH := $(shell uname)
ifneq ($(ARCH),Darwin)
  LDFLAGS += -lpthread -lrt -static-libasan
endif

PROXY_OBJ := webproxy.o steque.o

all: webproxy simplecached

webproxy: $(PROXY_OBJ) handle_with_cache.o handle_with_curl.o shm_channel.o gfserver.o 
	$(CC) -o $@ $(CFLAGS) $(CURL_CFLAGS) $^ $(LDFLAGS) $(CURL_LIBS)

simplecached: simplecache.o simplecached.o shm_channel.o steque.o
	$(CC) -o $@ $(CFLAGS) $^ $(LDFLAGS)

.PHONY: clean

clean:
	mv gfserver.o gfserver.tmpo 
	rm -rf *.o webproxy simplecached
	mv gfserver.tmpo gfserver.o  	
