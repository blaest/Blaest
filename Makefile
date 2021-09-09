CC=c89
CFLAGS=-std=c89 -o blaest 
CWARN=-Wall -Wextra -Wpedantic -Wno-format
PREFIX=/usr/local
INCLUDE_PATH=/usr/local/lib/blaest/include

blaest: src/blaest.c
	$(CC) $(CFLAGS) -O2 $(CWARN) src/blaest.c
	strip blaest

.PHONY: install
install: blaest
	cp blaest $(PREFIX)/bin
	mkdir -p $(INCLUDE_PATH)
	cp include/* $(INCLUDE_PATH)

.PHONY: uninstall
uninstall:
	rm $(PREFIX)/bin/blaest
	rm -rf $(INCLUDE_PATH)

.PHONY: clean
clean:
	rm blaest

.PHONY: debug
debug:
	$(CC) $(CFLAGS) -O0 -g -D_DEBUG $(CWARN) src/blaest.c

