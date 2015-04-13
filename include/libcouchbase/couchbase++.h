//! @file lcb++.h - Couchbase++ API
#ifndef LCB_PLUSPLUS_H
#define LCB_PLUSPLUS_H

#include <libcouchbase/couchbase.h>
#include <libcouchbase/api3.h>
#include <libcouchbase/pktfwd.h>
#include <libcouchbase/views.h>
#include <string>
#include <stdint.h>
#include <map>
#include <vector>
#include <iostream>
#include <libcouchbase/couchbase++/internal.h>

namespace Couchbase {


//! @brief Status code. This wrapp an `lcb_error_t`.
//!
//! @details
//! This is returned from higher level APIs, but may wrap around any
//! lcb_error_t by simply doing
//! @code{c++}
//! Couchbase::Status s(LCB_KEY_ENOENT) ;
//! @endcode
class Status {
public:
    Status(lcb_error_t rc=LCB_SUCCESS) : code(rc) { }

    //! Check status as boolean
    //! @return true or false if operation failed or succeeded
    bool success() const { return code == LCB_SUCCESS; }

    //! If the error was caused by network issues
    //! @return true if network error
    bool isNetworkError() const { return LCB_EIFNET(code) != 0; }

    //! If error was caused by bad user input (e.g. bad parameters)
    //! @return true if input error
    bool isInputError() const { return LCB_EIFINPUT(code) != 0; }

    //! If this is a data error (missing, not found, already exists)
    //! @return true if data error
    bool isDataError() const { return LCB_EIFDATA(code) != 0; }

    //! If this is a transient error which may go away on its own
    //! @return true if transient
    bool isTemporary() const { return LCB_EIFTMP(code) != 0; }

    //! Get textual description of error (uses lcb_strerror())
    //! @return The error string. Do not free the return value
    const char *description() const { return lcb_strerror(NULL, code); }

    //! Get the raw lcb_error_t value
    //! @return the error code
    lcb_error_t errcode() const { return code; }

    operator bool() const { return success(); }
    operator lcb_error_t() const { return code; }
//    operator const char * () const { return  description(); }
    Status(const Status& other) : code(other.code) {}
private:
    lcb_error_t code;
};
}

namespace std {
inline ostream& operator<< (ostream& os, const Couchbase::Status& obj) {
    os << "[0x" << std::hex << obj.errcode() << "]: " << obj.description();
    return os;
}
}

namespace Couchbase {
//! Base class for all commands.
class Command {
public:
    //! Set the key for the command
    //! @param buf the buffer for the key
    //! @param len the size of the key
    //! @note The buffer must remain valid until the operation has been
    //! scheduled.
    void key(const char *buf, size_t len) { LCB_CMD_SET_KEY(&m_base, buf, len); }
    void key(const char *buf) { key(buf, strlen(buf)); }
    void key(const std::string& s) { key(s.c_str(), s.size()); }

    const char *get_keybuf() const { return (const char *)m_base.key.contig.bytes; }
    size_t get_keylen() const { return m_base.key.contig.nbytes; }

    //! @brief Set the expiry for the command.
    //! @param n Expiry in seconds, and is either an offset in seconds from the
    //! current time, or an absolute Unix timestamp, depending on whether
    //! the number of seconds exceeds thirty days.
    //! @note Not all operations accept expiration. In these cases it will
    //!       be ignored.
    void expiry(unsigned n) { m_base.exptime = n; }

    //! @brief Set the CAS for the command.
    //! The CAS is typically obtained by the result of another operation.
    //! @note Not all operations accept a CAS.
    //! @param casval the CAS
    void cas(uint64_t casval) { m_base.cas = casval; }
    const lcb_CMDBASE* asCmdBase() const { return &m_base; }
    Command() { memset(this, 0, sizeof *this); }

    LCB_CXX_CMD_IS(lcb_CMDBASE, m_base)

protected:
    union { LCB_CXX_UREQUESTS };
};

//! @class GetCommand
//! @brief Command structure for retrieving items
class GetCommand : public Command {
public:
    LCB_CXX_CMD_CTOR(GetCommand)
    LCB_CXX_CMD_IS(lcb_CMDGET, m_get)
    //! @brief Make this get operation be a get-and-lock operation
    //! Set this value to the time the lock will be held. Operations to mutate
    //! the key while the lock is still held on the server will fail. You may
    //! unlock the item using @ref UnlockCommand.
    void locktime(unsigned n) { expiry(n); m_get.lock = 1; }
};

//! @brief Command structure for mutating/storing items
class StoreCommand : public Command {
public:
    //! @brief Create a new storage operation.
    //! @param op Type of mutation to perform.
    //! The default is @ref LCB_SET which unconditionally stores the item

    StoreCommand(lcb_storage_t op = LCB_SET) : Command() {
        m_store.operation = op;
    }

    StoreCommand(const std::string& key, const std::string& value, lcb_storage_t mode = LCB_SET) {
        this->key(key);
        this->value(value);
        this->mode(mode);
    }

    StoreCommand(const char *key, const char *value, lcb_storage_t mode = LCB_SET) {
        this->key(key);
        this->value(value);
        this->mode(mode);
    }
    //! @brief Explicitly set the mutation type
    //! @param op the mutation type.
    void mode(lcb_storage_t op) { m_store.operation = op; }

    //! @brief Set the value to be stored.
    //! @param b Buffer
    //! @param n Buffer length
    //! @note The buffer must remain valid until the operation has been
    //!       scheduled.
    void value(const void *b, size_t n) { LCB_CMD_SET_VALUE(&m_store, b, n); }
    void value(const char *s) { value(s, strlen(s)); }
    void value(const std::string& s) { value(s.c_str(), s.size()); }

    //! @brief Set the item's metadata flags.
    //! @param f the 32 bit flags value.
    //! @warning These flags are typically used by higher level clients to
    //!          determine the higher level datatype stored under the item.
    //!          Ensure to comply with these rules if inter-operability with
    //!          them is important.
    void itemflags(uint32_t f) { m_store.flags = f; }
    LCB_CXX_CMD_IS(lcb_CMDSTORE, m_store);
};

//! Command structure for atomic counter operations.
//! This command treats a value as a number (the value must be formatted as a
//! series of ASCII digis).
class CounterCommand : public Command {
public:
    //! Initialize the command.
    //! @param delta the delta to add to the value. If negative, the value is
    //! decremented.
    CounterCommand(int64_t delta = 1) : Command() { m_arith.delta = delta; }

    //! Provide a default value in case the item does not exist
    //! @param d the default value. If the item does not exist, this value
    //! will be used, and the delta value will not be applied. If the item
    //! does exist, this value will be ignored.
    void deflval(uint64_t d) { m_arith.initial = d; m_arith.create = 1; }

    //! Explicitly set the delta
    //! @param delta the delta to add (or subtract) to/from the current value.
    void delta(int64_t delta) { m_arith.delta = delta; }
    LCB_CXX_CMD_IS(lcb_CMDCOUNTER, m_arith);
};

//! Command structure for _removing_ items from the cluster.
class RemoveCommand : public Command {
public:
    LCB_CXX_CMD_CTOR(RemoveCommand)
    LCB_CXX_CMD_IS(lcb_CMDREMOVE, m_base)
};


class ObserveCommand : public Command {
public:
    LCB_CXX_CMD_CTOR(ObserveCommand)
    LCB_CXX_CMD_IS(lcb_CMDOBSERVE, m_obs);
    void master_only(bool val=true) {
        if (val) { m_obs.cmdflags |= LCB_CMDOBSERVE_F_MASTER_ONLY; }
        else { m_obs.cmdflags &= ~LCB_CMDOBSERVE_F_MASTER_ONLY; }
    }
};


typedef Command TouchCommand;

class Client;

//! @brief Base class for response objects.
struct ResponseBase {
    Internal::uResponses u;
    //! Get status
    //! @return the status of the operation.
    Status status() const { return u.base.rc; }

    //! Determine if response is final
    //! @return true if this is the final response, or if there are more
    //!         responses for the given command.
    bool final() const { return u.base.rflags & LCB_RESP_F_FINAL; }

    //! Get user associated data for the command
    //! @return the user data
    void *cookie() const { return (void *)u.base.cookie; }

    //! Get the CAS for the operation. Only valid if the operation succeeded,
    //! @return the CAS.
    uint64_t cas() const { return u.base.cas; }

    void fail(Status& rv) { u.base.rc = rv; }
};

class Response : public ResponseBase {
    friend class Client;

public:
    Response() {}
    template <class T> static T& setcode(T& r, Status& c) {
        r.u.base.rc = c;
        return r;
    }
    virtual void init(const lcb_RESPBASE *res) { u.base = *res; }
    virtual bool done() const { return true; }
    virtual ~Response() {}
private:
};

typedef Response StoreResponse;
typedef Response RemoveResponse;
typedef Response TouchResponse;

//! @brief Response for @ref GetCommand requests
class GetResponse : public Response {
public:
    inline GetResponse();
    ~GetResponse() { clear(); }

    //! @brief Release memory used by the response value
    void clear();

    //! Get the value for the item
    //! @return a buffer holding the value of the buffer. This buffer is valid
    //!         until the response is destroyed or the ::clear() function is
    //!         explicitly called.
    const char* valuebuf() const { return (const char *)u.get.value; }

    //! Gets the length of the value
    size_t valuesize() const { return u.get.nvalue; }

    //! Copies the contents of the value into a std::string
    inline void value(std::string& s) const;

    //! Appends the contents of the value into a std::vector<char>
    inline void value(std::vector<char>&v) const;

    std::string value() const { std::string s; value(s); return s; }

    //! Get the flags of the item. See StoreCommand::itemflags
    uint32_t valueflags() const { return u.get.itmflags; }
    uint32_t itemflags() const { return valueflags(); }

    //! @private
    void init(const lcb_RESPBASE *);

private:
    friend class Client;
    friend class ViewRow;
    void assign_first(const GetResponse& other);

    GetResponse(const GetResponse& other) { assign_first(other); }
    inline GetResponse& operator=(const GetResponse&);
    inline bool hasSharedBuffer() const;
    inline bool hasAllocBuffer() const;
    inline char *vbuf_refcnt();
};

class StatsResponse : public Response {
public:
    StatsResponse() : Response(), initialized(false), m_done(false) { }
    void init(const lcb_RESPBASE *resp);
    bool done() const { return m_done; }
    std::map<std::string,std::map<std::string,std::string> > stats;
private:
    bool initialized;
    bool m_done;
};

class CounterResponse : public Response {
public:
    void init(const lcb_RESPBASE *res) { u.arith = *(lcb_RESPCOUNTER *)res; }

    //! Get the current counter value
    //! @returns the current counter value. This is the value after the
    //!          counter operation. Only valid if the operation succeeded.
    uint64_t value() const { return u.arith.value; }
};

class ObserveResponse : public Response {
public:
    struct ServerReply {
        uint64_t cas;
        bool master;
        uint8_t status;
        ServerReply() : cas(0), master(false), status(0) {}
        bool exists() const { return status == LCB_OBSERVE_FOUND; }
        bool persisted() const { return status & (int)LCB_OBSERVE_PERSISTED; }
    };

    ObserveResponse() : Response(), initialized(false) { }
    void init(const lcb_RESPBASE *res);
    const ServerReply& master_reply() const;
    const std::vector<ServerReply>& all_replies() const { return sinfo; }
private:
    bool initialized;
    std::vector<ServerReply> sinfo;
};

//! @brief A BatchContext can be used to efficiently batch multiple operations.
//! @details
//! (aka "Bulk operations"). To use a batch context, first initialize it,
//! and then call each @ref Operation's Operation::submit() function (passing
//! it the batch object). Once all objects have been batched, you may call
//! the #submit() method (which submits the entire batch to be scheduled),
//! and finally, Client::wait().
//!
//! A batch client is considered *active* on the client when it has been
//! initialized. It is active until either the #submit() or #bail() functions
//! have been called. A batch context may be re-used by calling the #reset()
//! method which will make it active again (using the @ref Client passed to
//! the constructor).
//!
//! @note Operations submitted to the batch context are not actually submitted
//! to the network until Client::wait() is called.
class BatchContext {
public:
    //! @brief Initialize the batch client.
    //! @param client the client to use.
    //! @warning Only one batch context can be active on a client at a given
    //!          time. In addition, when a batch context is active, higher
    //!          level calls on the client (such as Client::get()) cannot be
    //!          performed.
    BatchContext(Client& client);
    ~BatchContext();

    //! Wrapper method for scheduling a get operation. This allows the operation
    //! to later be retrieved using #valueFor()
    Status get(const std::string& s);

    //! @brief Cancel any operations submitted in the current batch. This also
    //!        deactivates the batch. This is useful if scheduling several
    //!        dependent operations, where one of the operations fail.
    void bail();

    template <typename T> Status addop(T& op) {
        Status rv = op.scheduleLcb(getLcbt());
        if (rv) {
            m_remaining++;
        }
        return rv;
    }

    //! @brief Submit all previously scheduled operations. These operations
    //!        will be performed when Client::wait() is called. This function
    //!        also deactivates the batch.
    void submit();

    //! @brief Re-activates the batch.
    void reset();

    //! @brief Retrieve responses for @ref GetOperation objects which were requested
    //! via #get()
    //! @param s the key
    //! @return The GetResponse object for the given item. This is only valid
    //! if `s` was requested via e.g. `get(s)` _and_ #submit() has been called
    //! _and_ Client::wait() has been called.
    const GetResponse& valueFor(const std::string& s);
    const GetResponse& operator[](const std::string&s) { return valueFor(s); }
    inline lcb_t getLcbt() const;
private:
    friend class Command;
    bool entered;
    size_t m_remaining;
    Client& parent;
    Response dummy;
    std::map<std::string,Operation<GetCommand,GetResponse>*> resps;
};

//! @brief Main client object.
//! @details
//! The client object represents a connection to a _bucket_ on the cluster.
//! To connect to the bucket, simply initialize the client object with the
//! connection string for the bucket and the password (if protected).
//!
//! Once initialized, you may call the #connect() function to actually connect
//! and bootstrap the client.
class Client {
public:
    //! @brief Initialize the client
    //! @param connstr the connection string
    //! @param passwd the password for the bucket (if password protected)
    Client(const char *connstr = "couchbase://localhost/default", const char *passwd = NULL);
    ~Client();

    //! @brief Wait until client is connected
    //! @details
    //! This function will attempt to bootstrap the client. It will return a
    //! status code indicating whether the bootstrap was successful or not.
    //! If the bootstrap failed, you should destroy the object - as it cannot
    //! be used for operations.
    Status connect();

    //! Wait for all scheduled operations to complete. This is where the library
    //! sends  requests to the server and receives their responses
    void wait();

    //! Retrieve the inner `lcb_t` for use with the C API.
    //! @return the C library handle
    lcb_t getLcbt() const { return instance; }

    //! @private
    void breakout() { if (!remaining) { lcb_breakout(instance); } }
    //! @private
    void _dispatch(int, const lcb_RESPBASE*);
private:
    friend class BatchContext;
    friend class DurabilityContext;
    Status schedule(const Command&, Response*, lcb_CALLBACKTYPE op);
    lcb_t instance;
    size_t remaining;
    Client(Client&);
};

//! @class Operation
//! @brief Base "Operation" class
//! @details
//! This operation class is used to perform operations on the cluster. The
//! operation class contains both the command and response structure
//! for the operation.
//!
//! The operation class is simply a "subclass" of the command structure with
//! a placeholder structure for the response.
template <typename C=Command, typename R=Response>
class Operation : public C {
protected:
    typedef C _Cmdcls;
    typedef R _Respcls;

public:
    LCB_CXX_OP_CTOR(Operation)
    Operation<StoreCommand,R>(const std::string& k, const std::string& v,
        lcb_storage_t mode = LCB_SET) : StoreCommand(k, v, mode) {
    }
    Operation<StoreCommand,R>(const char *k, const char *v, lcb_storage_t mode = LCB_SET) :
            StoreCommand(k, v, mode) {
    }
    Operation<StoreCommand,R>(lcb_storage_t op) : StoreCommand(op) {}

    //! Get the response
    //! @return a reference to the inner response object
    R& response() { return res; }
    const R& const_response() const { return res; }

    //! Schedule an operation on an active context
    //! @param ctx an active context
    //! @return a status. If the status is not successful, this operation
    //!         has not been scheduled successfully. Otherwise,
    //!         the operation will complete once BatchContext::submit() and
    //!         Client::wait() have been invoked.
    Status schedule(BatchContext& ctx) { return ctx.addop(*this); }

    //! Execute this operation immediately on the client.
    //! @param client the client to use. The client must not have an active
    //!        context.
    //! @return the status of the response
    inline R& run(Client& client);
protected:
    friend class BatchContext;
    inline Status scheduleLcb(lcb_t);
    R res;
};

//! @class StoreOperation
//! Storage operation. @see StoreCommand
typedef Operation<StoreCommand, StoreResponse> StoreOperation;

//! @class GetOperation
//! @extends Operation
//! Retrieve item from the cluster. @see GetCommand
typedef Operation<GetCommand,GetResponse> GetOperation;

//! @class RemoveOperation
//! @extends Operation
//! @extends RemoveCommand
//! Remove/Delete operation. @see RemoveCommand
typedef Operation<RemoveCommand,RemoveResponse> RemoveOperation;

//! @class TouchOperation
//! @extends Operation
//! @extends TouchCommand
//! Touch operation (modify the expiry). @see TouchCommand
typedef Operation<TouchCommand,TouchResponse> TouchOperation;

//! @class CounterOperation
//! Change counter value. @see CounterCommand
typedef Operation<CounterCommand,CounterResponse> CounterOperation;
typedef Operation<ObserveCommand,ObserveResponse> ObserveOperation;

#define LCB_CXX_STORE_CTORS(cName, mType) \
    cName(const std::string k, const std::string& v) : StoreOperation(k, v, mType) { \
    } \
    cName(const char *k, const char *v) : StoreOperation(k, v, mType) { \
    } \
    cName(const char *key, size_t nkey, const char *value, size_t nvalue) : StoreOperation(mType) { \
        this->key(key, nkey); \
        this->value(value, nvalue); \
    }

lcb_t BatchContext::getLcbt() const { return parent.getLcbt(); }
} // namespace Couchbase

#include <libcouchbase/couchbase++/inldefs.h>

namespace Couchbase {
//! Modify item unconditionally
class UpsertOperation : public StoreOperation {
public:
    LCB_CXX_STORE_CTORS(UpsertOperation, LCB_SET);
};

//! Add item only if it does not exist
class AddOperation : public StoreOperation {
public:
    LCB_CXX_STORE_CTORS(AddOperation, LCB_ADD);
};

//! Replace item only if it exists
class ReplaceOperation : public StoreOperation {
public:
    LCB_CXX_STORE_CTORS(ReplaceOperation, LCB_REPLACE);
};
}


#endif
