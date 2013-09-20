all: sample

sample: examples/sample.cpp src/connection.cc
	$(CXX) -Wall -g -o $@ $^ \
		-I/sources/libcouchbase/inst/include \
		-Iinclude \
		-Wl,-rpath=/sources/libcouchbase/inst/lib \
		-L/sources/libcouchbase/inst/lib \
		-lcouchbase -lpthread

