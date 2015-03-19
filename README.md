# SPITZ

SPITZ is a reference implementation of the parallel programing model
SPITS (Scalable Partially Idempotent Tasks System). This allows you to
implement parallel programs that will scale in big heterogeneous
clusters and offer fault tolerance.

This is achieved by a C API where you only need to implement the
important functions for your problem, hiding the complexity of load
balancing, fault tolerance, task distribution, etc.

## Prerequisites

To compile and execute SPITZ you need the following programs:

 * C and C++ compiler
 * Make

To execute the Monitor and track the status of the computation,
you need the following:

 * Python 2.7
 * Kivy (Python lib)
 * Paramiko (Python lib)
 * Azure SDK (Python lib)

For Ubuntu/Debian:

    apt-get install make gcc g++ kivy python python-pip python-dev build-essential 

For CentOS/Redhat:

   yum install make gcc gcc-c++ python-kivy python python-pip python-dev build-essential


## Compiling

To compile SPITZ just issue:

    make

## Examples

There is a very simple example that calculates pi using Monte-Carlo
algorithm in the directory _examples_. To run the test, execute

    make pi 

Take a look at `examples/pi.c` to see how it was implemented. You might
also want to read `examples/Makefile` to understand the build process.

There is also an example that find the prime numbers in a range. To run
this, you have to options, running each member with gdb:
 
    make gdbx

Or without gdb, running as a normal program:

    make testx 

Replacing "x" with 0 (Job Manager), 1 (Committer), 2(Task Manager) or 
4 VM Task Manager). To use the monitor, see the next section.

## Monitor

To run the Monitor, make sure you installed all the libs required. You
need to have a job manager running and reachable. Just execute

	python monitor/Monitor.py

At the first time, you will have to complete the settings screen, but
the Monitor will store all this information in a text, loading at start
for the next time. Beware, as this information is stored in a plain text 
file.

## System wide installation

To install SPITZ in the default directory (/usr/local) issue the
following command:

    make install

If you want to change the prefix directory, issue:

    make install prefix=/usr

## Troubleshooting

### error while loading shared libraries: libspitz.so

This probably means that `libspitz.so` was installed in a unusual place,
so you need to set the `LD_LIBRARY_PATH` environment variable to point
to the installation directory like so:

    export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/path/to/install/dir/lib

### problem installing Kivy. 

Although a python library, Kivy may cause some trouble during installation. 
It has a package in pip, but this is not the recommended way for installing it. 
I recommend checking the kivy's installation page : 

http://kivy.org/docs/installation/installation-linux.html#using-software-packages.

### Monitor can't connect to job manager. 

Even when running all members locally, you might want to check the "Settings" page 
and make sure everything is correct. The node running the Job Manager must be
reachable for the Monitor one, with the right ports opened. Check the "Log" screen,
it might give you some idea of what's happening.

# Reporting bugs

If you find any bug you may report an issue in this repository. We will
be very happy to fix them!

<!-- vim:tw=72:sw=4:et:sta:spell
-->
