CFLAGS=-fPIC -g --std=gnu11
LDFLAGS=-rdynamic -pie -U_FORTIFY_SOURCE
LIBS=-lm

all: udp-local

udp-local: udp-local.c
	gcc $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

clean:
	rm -f udp-local