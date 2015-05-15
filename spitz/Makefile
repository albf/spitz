SRC = cfifo.c log.c comm.c main.c jobmanager.c committer.c taskmanager.c list.c md5.c monitor.c registry.c
COMM = comm

CPPFLAGS = -I.
CFLAGS = -g -Wall -Wextra 
LDFLAGS = -L. -lspitz -ldl -lrt -lpthread -lm

all: spitz

spitz: $(SRC) libspitz.so
	gcc $(CFLAGS) $(CPPFLAGS) -o $@ $(SRC) $(LDFLAGS)

libspitz.so: barray.c spitz.c
	$(CC) $(CFLAGS) -shared -fPIC -Wl,-soname,$@ -o $@ $^

clean:
	-rm -f spitz libspitz.so 

test:
	@true

prefix = /usr/local
libdir = $(prefix)/lib
bindir = $(prefix)/bin
incdir = $(prefix)/include

INST_INC = barray.h spitz.h stream.hpp spitz.hpp
INST_LIB = libspitz.so
INST_BIN = spitz

install: libspitz.so spitz
	install -m 755 -d $(DESTDIR)$(libdir)
	install -m 755 -d $(DESTDIR)$(bindir)
	install -m 755 -d $(DESTDIR)$(incdir)/spitz
	install -m 755 $(INST_LIB) $(DESTDIR)$(libdir)
	install -m 755 $(INST_BIN) $(DESTDIR)$(bindir)
	install -m 644 $(INST_INC) $(DESTDIR)$(incdir)/spitz

.PHONY: all clean
