all: liblcb++.so example

LCB_ROOT?=/sources/libcouchbase/inst
CPPFLAGS+=-g -Wall -Iinclude -I$(LCB_ROOT)/include
LDFLAGS+=-L$(LCB_ROOT)/lib -Wl,-rpath=$(LCB_ROOT)/lib -lcouchbase

liblcb++.so: src/lcb++.cpp
	$(CXX) $(CPPFLAGS) -shared -o $@ -fPIC $^ $(LDFLAGS)

example: example.cpp liblcb++.so
	$(CXX) $(CPPFLAGS) -o $@ $^ -L. -Wl,-rpath=. -llcb++ $(LDFLAGS)
