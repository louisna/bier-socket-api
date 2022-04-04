CFLAGS=-fPIC -g --std=gnu11
LDFLAGS=-rdynamic -pie -U_FORTIFY_SOURCE
INCLUDE_HEADERS_DIRECTORY=-Iheaders
LIBS=-lm

all: bier-bfr src/bier.o bier-sender.o

#bier-sender: bier-sender.c src/bier.o
#	gcc $(INCLUDE_HEADERS_DIRECTORY) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

bier-bfr: bier-bfr.c src/bier.o
	gcc $(INCLUDE_HEADERS_DIRECTORY) $(CFLAGS) $(LDFLAGS) -o $@ $^

%.o: %.c
	gcc $(INCLUDE_HEADERS_DIRECTORY) $(CFLAGS) $(LDFLAGS) -o $@ -c $^

clean:
	rm -f src/*.o *.o bier-bfr bier-sender
