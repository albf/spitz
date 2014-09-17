CPPFLAGS = -I..
CFLAGS = -shared -Wall -Wextra -fPIC -g3 -O0
CXXFLAGS = $(CFLAGS)
LDFLAGS = -L../spitz -lspitz

all: pi.so picpp.so prime.so

prime.so: prime.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $^ $(LDFLAGS)

pi.so: pi.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $^ $(LDFLAGS)

picpp.so: pi.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $@ $^ $(LDFLAGS)

test: pi.so picpp.so
	# Set some environment variables
	LD_LIBRARY_PATH=$$PWD/../spitz PATH=$$PATH:$$PWD/../spitz \
			gdb --args ./spitz 0 127.0.0.1 /home/alexandre/Codes/spitz/examples/pi.so 100
	
			# gdb --args ./spitz 2 127.0.0.1 $$PWD/pi.so 30
	#LD_LIBRARY_PATH=$$PWD/../spitz PATH=$$PATH:$$PWD/../spitz \
	#		mpirun -np 3 spitz $$PWD/picpp.so 10000
	# format : ./spitz #type #ipManager #Path.so #TotalJobs

debug: pi.so picpp.so
	export SPITS_LOG_LEVEL=2
	# Set some environment variables
	LD_LIBRARY_PATH=$$PWD/../spitz PATH=$$PATH:$$PWD/../spitz \
			mpirun -np 3 xterm -e gdb --args spitz $$PWD/pi.so 10000
	LD_LIBRARY_PATH=$$PWD/../spitz PATH=$$PATH:$$PWD/../spitz \
			mpirun -np 3 xterm -e gdb --args spitz $$PWD/picpp.so 10000



clean:
	-rm -f pi.so picpp.so

prefix = /usr/local
datadir = $(prefix)/share/spitz

install:
	install -m 0755 -d $(DESTDIR)$(datadir)
	install -m 0644 pi.c pi.cpp Makefile $(DESTDIR)$(datadir)
