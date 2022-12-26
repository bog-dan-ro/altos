CC=gcc
CXX=g++
RM=rm -f
CPPFLAGS=-g -std=c++17 $(shell pkg-config --cflags fuse3)
LDLIBS=-std=c++17 $(shell pkg-config --libs fuse3)

SRCS=main.cpp s52k.cpp
OBJS=$(subst .cpp,.o,$(SRCS))

all: s5fs

s5fs: $(OBJS)
	$(CXX) $(LDFLAGS) -o s5fs $(OBJS) $(LDLIBS)

depend: .depend

.depend: $(SRCS)
	$(RM) ./.depend
	$(CXX) $(CPPFLAGS) -MM $^>>./.depend;

clean:
	$(RM) $(OBJS)
	$(RM) s5fs

distclean: clean
	$(RM) *~ .depend

include .depend
