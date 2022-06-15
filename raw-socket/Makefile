CFLAGS=-I$(LIBDIR)/QCBOR/inc -g --std=gnu11
LIBDIR=libraries
LDFLAGS=-fPIC -rdynamic -pie -U_FORTIFY_SOURCE
INCLUDE_HEADERS_DIRECTORY=-Iinclude
LIBS=-L$(LIBDIR)/QCBOR -lqcbor  -lm
TFLAGS=-lcunit

all: libs bier-bfr bier-bifr src/udp-checksum.o src/local-processing.o src/qcbor-encoding.o src/bier.o src/bier-sender.o

bier-bfr: bier-bfr.c src/udp-checksum.o src/local-processing.o src/qcbor-encoding.o src/bier.o src/bier-sender.o
	gcc $(INCLUDE_HEADERS_DIRECTORY) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

bier-bifr: bier-bifr.c src/udp-checksum.o src/local-processing.o src/qcbor-encoding.o src/bier.o src/bier-sender.o
	gcc $(INCLUDE_HEADERS_DIRECTORY) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	gcc $(INCLUDE_HEADERS_DIRECTORY) $(CFLAGS) $(LDFLAGS) -o $@ -c $^

sender: sender.c src/qcbor-encoding.o
	gcc $(INCLUDE_HEADERS_DIRECTORY) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

test: tests/test_bier tests/test_cbor

tests/%: tests/%.c src/bier.o src/qcbor-encoding.o
	gcc $(INCLUDE_HEADERS_DIRECTORY) $(CFLAGS) -o $@ $^ $(LIBS) -lcunit
	./$@
	rm $@

$(LIBDIR)/QCBOR/libqcbor.a:
	make -C $(LIBDIR)/QCBOR

libs: $(LIBDIR)/QCBOR/libqcbor.a

clean:
	rm -f src/*.o *.o bier-bfr bier-bifr tests/test_bier tests/test_cbor