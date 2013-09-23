LCB_ROOT=/sources/libcouchbase/inst
CPPFLAGS=-Wall -g -I$(LCB_ROOT)/include -Iinclude
LDFLAGS=-Wl,-rpath=$(LCB_ROOT)/lib -L$(LCB_ROOT)/lib \
		-Wl,-rpath='$(ORIGIN)' -L. \
		-lcouchbase -lpthread -lcouchbase-cxx

SO=libcouchbase-cxx.so
OBJS=src/connection.o src/mt.o src/future.o

all: $(SO) sample mt

%.o: %.cc
	$(CXX) -c $(CPPFLAGS) -fPIC -o $@ $^

libcouchbase-cxx.so: $(OBJS)
	$(CXX) $(CPPFLAGS) -shared -o $@ $^


sample: examples/sample.cpp $(SO)
	$(CXX) $(CPPFLAGS) -o $@ $^ $(LDFLAGS)

mt: examples/threads.cc examples/cliopts.o $(SO)
	$(CXX) $(CPPFLAGS) -o $@ $^ $(LDFLAGS)
