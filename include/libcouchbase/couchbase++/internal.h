/**
 * This file contains internal definitions for use by the main API header file.
 */

// This macro overloads the address-of operator so that it is castable to its
// native libcouchbase type
//#define LCB_CXX_CMD_IS(name, memb) const name* operator&() const { return &memb; }

// This macro implements the 'easy constructors' using keys
#define LCB_CXX_CMD_CTOR(name) \
    name() : Command() {} \
    name(const char *k) : Command() {key(k);} \
    name(const char *k, size_t n) : Command() {key(k,n);} \
    name(const std::string& s) : Command() {key(s);}

namespace Couchbase {
class Client;
class Status;
class BatchContext;
template <typename C, typename R> class Operation;
}
