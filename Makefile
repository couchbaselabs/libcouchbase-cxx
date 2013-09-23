LCB_ROOT=/sources/libcouchbase/inst
CPPFLAGS=-Wall -g -I$(LCB_ROOT)/include -Iinclude
LDFLAGS=-Wl,-rpath=$(LCB_ROOT)/lib -L$(LCB_ROOT)/lib -lcouchbase -lpthread

all: sample mt

sample: examples/sample.cpp src/connection.cc
	$(CXX) $(CPPFLAGS) -o $@ $^ $(LDFLAGS)

mt: examples/threads.cc src/connection.cc src/mt.cc src/future.cc
	$(CXX) $(CPPFLAGS) -o $@ $^ $(LDFLAGS)
