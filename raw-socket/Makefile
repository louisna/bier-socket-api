CFLAGS=-fPIC -g --std=gnu11
LDFLAGS=-rdynamic -pie -U_FORTIFY_SOURCE
INCLUDE_HEADERS_DIRECTORY=-Iheaders
LIBS=-lm

all: bier-bfr bier-bifr src/bier.o src/bier-sender.o

bier-bfr: bier-bfr.c src/bier.o
	gcc $(INCLUDE_HEADERS_DIRECTORY) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

bier-bifr: bier-bifr.c src/bier.o src/bier-sender.o
	gcc $(INCLUDE_HEADERS_DIRECTORY) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	gcc $(INCLUDE_HEADERS_DIRECTORY) $(CFLAGS) $(LDFLAGS) -o $@ -c $^

clean:
	rm -f src/*.o *.o bier-bfr bier-bifr
