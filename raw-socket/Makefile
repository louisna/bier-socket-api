CFLAGS=-I$(LIBDIR)/QCBOR/inc -g --std=gnu11
LIBDIR=libraries
LDFLAGS=-fPIC -rdynamic -pie -U_FORTIFY_SOURCE
INCLUDE_HEADERS_DIRECTORY=-Iinclude
LIBS=-L$(LIBDIR)/QCBOR -lqcbor  -lm
TFLAGS=-lcunit

all: libbier.a libs bier-bfr sender receiver src/udp-checksum.o src/qcbor-encoding.o src/bier.o src/bier-sender.o src/public_bier.o src/multicast.o

bier-bfr: bier-bfr.c src/udp-checksum.o src/qcbor-encoding.o src/bier.o src/bier-sender.o
	gcc $(INCLUDE_HEADERS_DIRECTORY) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	gcc $(INCLUDE_HEADERS_DIRECTORY) $(CFLAGS) $(LDFLAGS) -o $@ -c $^

sender: sender.c src/qcbor-encoding.o src/udp-checksum.o src/bier-sender.o src/multicast.o src/public_bier.o
	gcc $(INCLUDE_HEADERS_DIRECTORY) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

receiver: receiver.c src/public_bier.o
	gcc $(INCLUDE_HEADERS_DIRECTORY) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

test: tests/test_bier tests/test_cbor

tests/%: tests/%.c src/bier.o src/qcbor-encoding.o
	gcc $(INCLUDE_HEADERS_DIRECTORY) $(CFLAGS) -o $@ $^ $(LIBS) -lcunit
	./$@
	rm $@

$(LIBDIR)/QCBOR/libqcbor.a:
	make -C $(LIBDIR)/QCBOR

libbier.a: src/public_bier.o
	ar -r $@ $^

libs: $(LIBDIR)/QCBOR/libqcbor.a

clean:
	rm -f src/*.o *.o bier-bfr tests/test_bier tests/test_cbor sender receiver
