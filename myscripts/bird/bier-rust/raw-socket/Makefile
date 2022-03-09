CFLAGS=-fPIC -g --std=gnu11
LDFLAGS=-rdynamic -pie -U_FORTIFY_SOURCE

all: bier-sender bier-bfr

bier-sender:
	gcc $(CFLAGS) $(LDFLAGS) -o bier-sender bier-sender.c

bier-bfr:
	gcc $(CFLAGS) $(LDFLAGS) -o bier-bfr bier-bfr.c

clean:
	rm bier-bfr bier-sender
