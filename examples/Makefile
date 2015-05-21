CPPFLAGS = -I..
CFLAGS = -shared -Wall -Wextra -fPIC -g3 -O0
CXXFLAGS = $(CFLAGS)
LDFLAGS = -L../spitz -lspitz

TASKS = 35
LIBSO = prime.so

all: pi.so picpp.so prime.so

prime.so: prime.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $^ $(LDFLAGS) -lm

pi.so: pi.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $^ $(LDFLAGS)

picpp.so: pi.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $@ $^ $(LDFLAGS)

python:
	python monitor.py

fixguys:
	rm spitz
	rm libspitz.so
	cp ../spitz/spitz .
	cp ../spitz/libspitz.so .

valgrind0: pi.so
	# Set some environment variables
	LD_LIBRARY_PATH=$$LD_LIBRARY_PATH:$$PWD:$$PWD/../spitz PATH=$$PATH:$$PWD/../spitz \
		valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all ./spitz 0 127.0.0.1 $(LIBSO) $(TASKS)

valgrind1: pi.so
	# Set some environment variables
	LD_LIBRARY_PATH=$$LD_LIBRARY_PATH:$$PWD:$$PWD/../spitz PATH=$$PATH:$$PWD/../spitz \
		valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all ./spitz 1 127.0.0.1 $(LIBSO) $(TASKS)

valgrind2: pi.so
	# Set some environment variables
	LD_LIBRARY_PATH=$$LD_LIBRARY_PATH:$$PWD:$$PWD/../spitz PATH=$$PATH:$$PWD/../spitz \
		valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all ./spitz 2 127.0.0.1 $(LIBSO) $(TASKS)

gdb0: pi.so
	LD_LIBRARY_PATH=$$LD_LIBRARY_PATH:$$PWD:$$PWD/../spitz PATH=$$PATH:$$PWD/../spitz \
			gdb -ex run --args ./spitz 0 127.0.0.1 $(LIBSO) $(TASKS)

gdb1: pi.so
	LD_LIBRARY_PATH=$$LD_LIBRARY_PATH:$$PWD:$$PWD/../spitz PATH=$$PATH:$$PWD/../spitz \
			gdb -ex run --args ./spitz 1 127.0.0.1 $(LIBSO) $(TASKS)

gdb2: pi.so
	LD_LIBRARY_PATH=$$LD_LIBRARY_PATH:$$PWD:$$PWD/../spitz PATH=$$PATH:$$PWD/../spitz \
			gdb -ex run --args ./spitz 2 127.0.0.1 $(LIBSO) $(TASKS)


gdb3: pi.so
	LD_LIBRARY_PATH=$$LD_LIBRARY_PATH:$$PWD:$$PWD/../spitz PATH=$$PATH:$$PWD/../spitz \
			gdb -ex run --args ./spitz 3 127.0.0.1 $(LIBSO) $(TASKS)

gdb4: pi.so
	LD_LIBRARY_PATH=$$LD_LIBRARY_PATH:$$PWD:$$PWD/../spitz PATH=$$PATH:$$PWD/../spitz \
			gdb -ex run --args ./spitz 4 127.0.0.1 $(LIBSO) $(TASKS)

test0: prime.so
	LD_LIBRARY_PATH=$$LD_LIBRARY_PATH:$$PWD:$$PWD/../spitz PATH=$$PATH:$$PWD/../spitz \
			./spitz 0 127.0.0.1 $(LIBSO) $(TASKS)

test1: prime.so
	LD_LIBRARY_PATH=$$LD_LIBRARY_PATH:$$PWD:$$PWD/../spitz PATH=$$PATH:$$PWD/../spitz \
			./spitz 1 127.0.0.1 $(LIBSO) $(TASKS)

test2: prime.so
	LD_LIBRARY_PATH=$$LD_LIBRARY_PATH:$$PWD:$$PWD/../spitz PATH=$$PATH:$$PWD/../spitz \
			./spitz 2 127.0.0.1 $(LIBSO) $(TASKS)

test4: prime.so
	LD_LIBRARY_PATH=$$LD_LIBRARY_PATH:$$PWD:$$PWD/../spitz PATH=$$PATH:$$PWD/../spitz \
			./spitz 4 127.0.0.1 $(LIBSO) $(TASKS)

debug: pi.so picpp.so
	export SPITS_LOG_LEVEL=2
	# Set some environment variables
	LD_LIBRARY_PATH=$$PWD/../spitz PATH=$$PATH:$$PWD/../spitz \
			mpirun -np 0 xterm -e gdb --args spitz $$PWD/pi.so $(TASKS)
	LD_LIBRARY_PATH=$$PWD/../spitz PATH=$$PATH:$$PWD/../spitz \
			mpirun -np 3 xterm -e gdb --args spitz $$PWD/picpp.so $(TASKS)



clean:
	-rm -f pi.so picpp.so prime.so

prefix = /usr/local
datadir = $(prefix)/share/spitz

install:
	install -m 0755 -d $(DESTDIR)$(datadir)
	install -m 0644 pi.c pi.cpp Makefile $(DESTDIR)$(datadir)
