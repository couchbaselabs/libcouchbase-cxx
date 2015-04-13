/**
 * This file contains internal definitions for use by the main API header file.
 */

// This macro overloads the address-of operator so that it is castable to its
// native libcouchbase type
#define LCB_CXX_CMD_IS(name, memb) const name* operator&() const { return &memb; }

// This macro implements the 'easy constructors' using keys
#define LCB_CXX_CMD_CTOR(name) \
    name() : Command() {} \
    name(const char *k) : Command() {key(k);} \
    name(const char *k, size_t n) : Command() {key(k,n);} \
    name(const std::string& s) : Command() {key(s);}

#define LCB_CXX_UREQUESTS \
    lcb_CMDBASE m_base; \
    lcb_CMDGET m_get; \
    lcb_CMDSTORE m_store; \
    lcb_CMDGETREPLICA m_rget; \
    lcb_CMDCOUNTER m_arith; \
    lcb_CMDSTATS m_stats; \
    lcb_CMDVERBOSITY m_verbosity; \
    lcb_CMDOBSERVE m_obs; \
    lcb_CMDTOUCH m_touch; \
    lcb_CMDUNLOCK m_unlock;

namespace Couchbase {

class Client;
class Command;
class Response;
class Status;
class BatchContext;
template <typename C, typename R> class Operation;

namespace Internal {
typedef union {
    lcb_RESPGET get;
    lcb_RESPBASE base;
    lcb_RESPCOUNTER arith;
    lcb_RESPSERVERBASE servers;
    lcb_RESPOBSERVE obs;
    lcb_RESPENDURE endure;
} uResponses;
}
}
