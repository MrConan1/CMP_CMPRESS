CC := gcc
CFLAGS :=
INSTALL := install
PREFIX := /usr/local
bindir := $(PREFIX)/bin

cmp_cmpress: compress_rtns.c cmp_cmpress.c
	$(CC) $(CFLAGS) compress_rtns.c cmp_cmpress.c -o $@

.PHONY: all clean install

all: cmp_cmpress

install: cmp_cmpress
	$(INSTALL) -d $(bindir)
	$(INSTALL) cmp_cmpress $(bindir)

clean:
	rm -f cmp_cmpress
