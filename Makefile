# Config Block
# =====================================

# Which C Compiler should be used
CC=gcc

# What flags should it have
CFLAGS=-std=gnu90 -o blaest

# What warnings should it have
CWARN=-Wall -Wextra -Wpedantic -Wno-format

# Where will Blaest be installed
PREFIX=/usr/local

# Where will it look for libraries included within <...>
INCLUDE_PATH=/usr/local/lib/blaest/include

#======================================
# The following options are very early in development
#======================================

# Enable Blaest's use of pthreads for threading
BLAEST_PTHREAD=0

# End Config Block
# =====================================
ifeq ($(BLAEST_PTHREAD), 1)
    CFLAGS +=-D_BLANG_USE_THREADS -D_BLANG_PTHREADS -lpthread -pthread 
endif

blaest: src/blaest.c
	$(CC) $(CFLAGS) -O2 $(CWARN) src/blaest.c
	strip blaest

.PHONY: install
install: blaest
	cp blaest $(PREFIX)/bin
	mkdir -p $(INCLUDE_PATH)
	cp -r include/* $(INCLUDE_PATH)

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

