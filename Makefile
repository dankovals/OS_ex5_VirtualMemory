CXX=g++
CXXFLAGS = -Wall -std=c++11 -g -I.

LIBSRC=VirtualMemory.cpp
LIBOBJ=$(LIBSRC:.cpp=.o)

VMLIB = libVirtualMemory.a
TARGETS = $(VMLIB)

TARNAME=ex5.tar
TARSRCS=VirtualMemory.cpp Makefile README  

all: $(TARGETS)

$(TARGETS): $(LIBOBJ)
	$(AR) $(ARFLAGS) $@ $^
	ranlib $@

clean:
	rm -f $(VMLIB) $(LIBOBJ) ex5.tar

tar:
	tar -cvf $(TARNAME) $(TARSRCS)
