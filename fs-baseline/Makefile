VERSION=0.01.00

CFLAGS += -Wall -Wextra -DVERSION='"$(VERSION)"'

OBJS =	fs-write-setup.o \
	fs-write-seq.o \
	fs-write-rnd.o \
	fs-write-many.o \
	fs-rewrite-seq.o \
	fs-read-setup.o \
	fs-read-seq.o \
	fs-read-rnd.o \
	fs-read-write-rnd.o \
	fs-noop.o \
	fs-dump-results.o \
	fs-test.o

fs-test: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -lm -lpthread -o $@ $(LDFLAGS)

clean:
	rm -f fs-test $(OBJS) fs-test.1.gz
	rm -f fs-test-$(VERSION).tar.gz
