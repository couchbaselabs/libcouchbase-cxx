# Couchbase++ - C++ Wrappers for [libcouchbase](https://github.com/couchbase/libcouchbase)

This file contains header-only C++ wrappers for libcouchbase. It is a work
in progress.

## Requirements

* [libcouchbase](https://github.com/couchbase/libcouchbase)
* A C++11 compiler (Sorry, there is no legacy C++ version yet)

## Building/Installing

Since this is a header-only library there is no need to install or build anything.
All components are preprocessor headers which are compiled with your own
source code.

There _is_ a `CMakeLists.txt` included, mainly for building examples and tests.

## Documentation

Documentation can be generated using [Doxygen](http://www.doxygen.org). To
generate the documentation, do the following:

```
make -f doc/Makefile public
```

The documentation index will then be available in HTML form in
`doc/public/html/index.html`.

Note that all documentation can also be accessed from within the headers.

## Examples

There is an `example.cpp` file in the repository root.
