VERSION=0.01.00

CFLAGS += -Wall -Wextra -DVERSION='"$(VERSION)"'

fs-test: fs-test.o
	$(CC) $(CFLAGS) $< -lm -lpthread -o $@ $(LDFLAGS) -g

clean:
	rm -f fs-test fs-test.o fs-test.1.gz
	rm -f fs-test-$(VERSION).tar.gz
