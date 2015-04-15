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

namespace Couchbase {

class Client;
class Status;
class BatchContext;
class DurabilityOptions;

namespace Internal {
    template <typename T> class MultiContextT;
    template<typename T> using MultiContext = MultiContextT<T>;
    typedef MultiContext<lcb_CMDENDURE> MultiDurContext;
    typedef MultiContext<lcb_CMDOBSERVE> MultiObsContext;
}

template <typename C, typename R> class Operation;

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
    operator const char * () const { return  description(); }
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
template <typename T>
class Command {
public:
    //! Set the key for the command
    //! @param buf the buffer for the key
    //! @param len the size of the key
    //! @note The buffer must remain valid until the operation has been
    //! scheduled.
    void key(const char *buf, size_t len) { LCB_CMD_SET_KEY(&m_cmd, buf, len); }
    void key(const char *buf) { key(buf, strlen(buf)); }
    void key(const std::string& s) { key(s.c_str(), s.size()); }

    const char *get_keybuf() const { return (const char *)m_cmd.key.contig.bytes; }
    size_t get_keylen() const { return m_cmd.key.contig.nbytes; }

    //! @brief Set the expiry for the command.
    //! @param n Expiry in seconds, and is either an offset in seconds from the
    //! current time, or an absolute Unix timestamp, depending on whether
    //! the number of seconds exceeds thirty days.
    //! @note Not all operations accept expiration. In these cases it will
    //!       be ignored.
    void expiry(unsigned n) { m_cmd.exptime = n; }

    //! @brief Set the CAS for the command.
    //! The CAS is typically obtained by the result of another operation.
    //! @note Not all operations accept a CAS.
    //! @param casval the CAS
    void cas(uint64_t casval) { m_cmd.cas = casval; }
    const lcb_CMDBASE* as_basecmd() const { return (const lcb_CMDBASE*)&m_cmd; }
    Command() { memset(this, 0, sizeof *this); }
    const T* operator&() const { return &m_cmd; }
protected:
    T m_cmd;
};

#define LCB_CXX_CMD_CTOR(name) \
    name() : Command() {} \
    name(const char *k) : Command() {key(k);} \
    name(const char *k, size_t n) : Command() {key(k,n);} \
    name(const std::string& s) : Command() {key(s);}

//! @class GetCommand
//! @brief Command structure for retrieving items
class GetCommand : public Command<lcb_CMDGET> {
public:
    LCB_CXX_CMD_CTOR(GetCommand)
    //! @brief Make this get operation be a get-and-lock operation
    //! Set this value to the time the lock will be held. Operations to mutate
    //! the key while the lock is still held on the server will fail. You may
    //! unlock the item using @ref UnlockCommand.
    void locktime(unsigned n) { expiry(n); m_cmd.lock = 1; }
};

//! @brief Command structure for mutating/storing items
class StoreCommand : public Command<lcb_CMDSTORE> {
public:
    //! @brief Create a new storage operation.
    //! @param op Type of mutation to perform.
    //! The default is @ref LCB_SET which unconditionally stores the item

    StoreCommand(lcb_storage_t op = LCB_SET) : Command() {
        m_cmd.operation = op;
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
    void mode(lcb_storage_t op) { m_cmd.operation = op; }

    //! @brief Set the value to be stored.
    //! @param b Buffer
    //! @param n Buffer length
    //! @note The buffer must remain valid until the operation has been
    //!       scheduled.
    void value(const void *b, size_t n) { LCB_CMD_SET_VALUE(&m_cmd, b, n); }
    void value(const char *s) { value(s, strlen(s)); }
    void value(const std::string& s) { value(s.c_str(), s.size()); }

    //! @brief Set the item's metadata flags.
    //! @param f the 32 bit flags value.
    //! @warning These flags are typically used by higher level clients to
    //!          determine the higher level datatype stored under the item.
    //!          Ensure to comply with these rules if inter-operability with
    //!          them is important.
    void itemflags(uint32_t f) { m_cmd.flags = f; }
};

//! Command structure for atomic counter operations.
//! This command treats a value as a number (the value must be formatted as a
//! series of ASCII digis).
class CounterCommand : public Command<lcb_CMDCOUNTER> {
public:
    //! Initialize the command.
    //! @param delta the delta to add to the value. If negative, the value is
    //! decremented.
    CounterCommand(int64_t delta = 1) : Command() { m_cmd.delta = delta; }

    //! Provide a default value in case the item does not exist
    //! @param d the default value. If the item does not exist, this value
    //! will be used, and the delta value will not be applied. If the item
    //! does exist, this value will be ignored.
    void deflval(uint64_t d) { m_cmd.initial = d; m_cmd.create = 1; }

    //! Explicitly set the delta
    //! @param delta the delta to add (or subtract) to/from the current value.
    void delta(int64_t delta) { m_cmd.delta = delta; }
};

//! Command structure for _removing_ items from the cluster.
class RemoveCommand : public Command<lcb_CMDREMOVE> {
public:
    LCB_CXX_CMD_CTOR(RemoveCommand)
};

class StatsCommand : public Command<lcb_CMDSTATS> {
public:
    LCB_CXX_CMD_CTOR(StatsCommand)
};

class TouchCommand : public Command<lcb_CMDTOUCH> {
public:
    LCB_CXX_CMD_CTOR(TouchCommand)
};

class UnlockCommand : public Command<lcb_CMDUNLOCK> {
public:
    LCB_CXX_CMD_CTOR(UnlockCommand)
};

class ObserveCommand : public Command<lcb_CMDOBSERVE> {
public:
    LCB_CXX_CMD_CTOR(ObserveCommand)
    void master_only(bool val=true) {
        if (val) {
            m_cmd.cmdflags |= LCB_CMDOBSERVE_F_MASTER_ONLY;
        } else {
            m_cmd.cmdflags &= ~LCB_CMDOBSERVE_F_MASTER_ONLY;
        }
    }
};


class Client;

//! Base handler class for handling/converting responses.
//! This is a fairly low level class and is wrapped upon by higher functionality.
//! The handler object is typically passed to one of the @ref Client schedule
//! functions. Normally this is done transparently.
class Handler {
public:
    //! Called for each response.
    //! @param client the client
    //! @param cbtype the type of response (one the `LCB_CALLBACK_*` constants)
    //! @param rb the response object (as received from the C library). This may
    //!        be cast to one of its subtypes.
    virtual void handle_response(Client& client, int cbtype, const lcb_RESPBASE *rb) = 0;

    //! Indicate whether this is the final command for the request
    //! @return true if done
    virtual bool done() const = 0;
    virtual ~Handler() {}
    Handler* as_cookie() { return this; }
};

//! @brief Base class for response objects.
template <typename T=lcb_RESPBASE>
class Response : public Handler {
public:
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

    Response() {}
    virtual ~Response() {}

    template <class D> static D& setcode(D& r, Status& c) {
        r.u.base.rc = c;
        return r;
    }

    virtual void handle_response(Client&, int, const lcb_RESPBASE *res) override {
        u.resp = *reinterpret_cast<const T*>(res);
    }

    virtual bool done() const override {
        return true;
    }

protected:
    union {
        lcb_RESPBASE base;
        T resp;
    } u;
};

namespace Internal {
typedef Response<lcb_RESPBASE> BaseResponse;
}

typedef Response<lcb_RESPSTORE> StoreResponse;
typedef Response<lcb_RESPREMOVE> RemoveResponse;
typedef Response<lcb_RESPTOUCH> TouchResponse;
typedef Response<lcb_RESPUNLOCK> UnlockResponse;

//! @brief Response for @ref GetCommand requests
class GetResponse : public Response<lcb_RESPGET> {
public:
    inline GetResponse();
    GetResponse(const GetResponse& other) { assign_first(other); }
    inline GetResponse& operator=(const GetResponse&);

    ~GetResponse() { clear(); }

    //! @brief Release memory used by the response value
    void clear();

    //! Get the value for the item
    //! @return a buffer holding the value of the buffer. This buffer is valid
    //!         until the response is destroyed or the ::clear() function is
    //!         explicitly called.
    const char* valuebuf() const { return (const char *)u.resp.value; }

    //! Gets the length of the value
    size_t valuesize() const { return u.resp.nvalue; }

    //! Copies the contents of the value into a std::string
    inline void value(std::string& s) const;

    //! Appends the contents of the value into a std::vector<char>
    inline void value(std::vector<char>&v) const;

    std::string value() const { std::string s; value(s); return s; }

    //! Get the flags of the item. See StoreCommand::itemflags
    uint32_t valueflags() const { return u.resp.itmflags; }
    uint32_t itemflags() const { return valueflags(); }

    //! @private
    void handle_response(Client&, int, const lcb_RESPBASE *) override;

private:
    friend class Client;
    friend class ViewRow;
    void assign_first(const GetResponse& other);

    inline bool has_shared_buffer() const;
    inline bool has_alloc_buffer() const;
    inline char *vbuf_refcnt();
};

class StatsResponse : public Response<lcb_RESPSTATS> {
public:
    StatsResponse() : Response(), initialized(false), m_done(false) { }
    void handle_response(Client&, int, const lcb_RESPBASE *resp) override;
    bool done() const override { return m_done; }
    std::map<std::string,std::map<std::string,std::string> > stats;
private:
    bool initialized;
    bool m_done;
};

class CounterResponse : public Response<lcb_RESPCOUNTER> {
public:
    void handle_response(Client&, int, const lcb_RESPBASE *res) override {
        u.resp = *(lcb_RESPCOUNTER *)res;
    }

    //! Get the current counter value
    //! @returns the current counter value. This is the value after the
    //!          counter operation. Only valid if the operation succeeded.
    uint64_t value() const { return u.resp.value; }
};

class ObserveResponse : public Response<lcb_RESPOBSERVE> {
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
    inline void handle_response(Client&, int, const lcb_RESPBASE *res) override;
    inline const ServerReply& master_reply() const;
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
    inline BatchContext(Client& client);
    inline ~BatchContext();

    //! Wrapper method for scheduling a get operation. This allows the operation
    //! to later be retrieved using #valueFor()
    inline Status get(const std::string& s);

    //! @brief Cancel any operations submitted in the current batch. This also
    //!        deactivates the batch. This is useful if scheduling several
    //!        dependent operations, where one of the operations fail.
    inline void bail();

    template <typename T> Status addop(T& op) {
        Status rv = op.schedule_cli(parent);
        if (rv) {
            m_remaining++;
        }
        return rv;
    }

    //! @brief Submit all previously scheduled operations. These operations
    //!        will be performed when Client::wait() is called. This function
    //!        also deactivates the batch.
    inline void submit();

    //! @brief Re-activates the batch.
    inline void reset();

    //! @brief Retrieve responses for @ref GetOperation objects which were requested
    //! via #get()
    //! @param s the key
    //! @return The GetResponse object for the given item. This is only valid
    //! if `s` was requested via e.g. `get(s)` _and_ #submit() has been called
    //! _and_ Client::wait() has been called.
    inline const GetResponse& value_for(const std::string& s);
    inline const GetResponse& operator[](const std::string&s) { return value_for(s); }
    inline lcb_t handle() const;
private:
    bool entered;
    size_t m_remaining;
    Client& parent;
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
    inline Client(const std::string& connstr = "couchbase://localhost/default", const std::string& passwd = "");
    inline ~Client();

    inline GetResponse get(const GetCommand&);
    inline TouchResponse touch(const TouchCommand&);
    inline StoreResponse upsert(const StoreCommand&);
    inline StoreResponse add(const StoreCommand&);
    inline StoreResponse replace(const StoreCommand&);
    inline RemoveResponse remove(const RemoveCommand&);
    inline CounterResponse counter(const CounterCommand&);
    inline StatsResponse stats(const std::string& key);
    inline UnlockResponse unlock(const UnlockCommand& cmd);

    //! @brief Wait until client is connected
    //! @details
    //! This function will attempt to bootstrap the client. It will return a
    //! status code indicating whether the bootstrap was successful or not.
    //! If the bootstrap failed, you should destroy the object - as it cannot
    //! be used for operations.
    inline Status connect();

    //! Wait for all scheduled operations to complete. This is where the library
    //! sends  requests to the server and receives their responses
    inline void wait();

    //! Retrieve the inner `lcb_t` for use with the C API.
    //! @return the C library handle
    inline lcb_t handle() const { return m_instance; }
    inline lcb_t getLcb() const { return handle(); }

    //! @private
    inline void breakout() { if (!remaining) { lcb_breakout(m_instance); } }
    //! @private
    inline void _dispatch(int, const lcb_RESPBASE*);

    //! @private
    Status schedule_get(const lcb_CMDGET *cmd, Handler *handler) {
        return lcb_get3(m_instance, handler->as_cookie(), cmd);
    }
    Status schedule_store(const lcb_CMDSTORE *cmd, Handler *handler) {
        return lcb_store3(m_instance, handler->as_cookie(), cmd);
    }
    Status schedule_touch(const lcb_CMDTOUCH *cmd, Handler *handler) {
        return lcb_touch3(m_instance, handler->as_cookie(), cmd);
    }
    Status schedule_counter(const lcb_CMDCOUNTER *cmd, Handler *handler) {
        return lcb_counter3(m_instance, handler->as_cookie(), cmd);
    }
    Status schedule_remove(const lcb_CMDREMOVE *cmd, Handler *handler) {
        return lcb_remove3(m_instance, handler->as_cookie(), cmd);
    }
    Status schedule_unlock(const lcb_CMDUNLOCK *cmd, Handler *handler) {
        return lcb_unlock3(m_instance, handler->as_cookie(), cmd);
    }
    Status schedule_stats(const lcb_CMDSTATS *cmd, Handler *handler) {
        return lcb_stats3(m_instance, handler->as_cookie(), cmd);
    }

    Status mctx_endure(const DurabilityOptions&, Handler*, Internal::MultiDurContext&);
    Status mctx_observe(Handler*, Internal::MultiObsContext&);

    void enter() { lcb_sched_enter(m_instance); }
    void leave() { lcb_sched_leave(m_instance); }
    void fail() { lcb_sched_fail(m_instance); }

private:
    friend class BatchContext;
    friend class DurabilityContext;
    lcb_t m_instance;
    size_t remaining;
    Client(Client&) = delete;
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
template <typename C=Command<lcb_CMDBASE>, typename R=Response<lcb_RESPBASE>>
class Operation : public C {
protected:
    typedef C CommandType;
    typedef R ResponseType;

public:
    Operation() : CommandType() {}
    Operation(const CommandType& c) : CommandType(c) {}
    Operation(CommandType& c) : CommandType(c) {}
    Operation(const char *key) : CommandType(key) {}
    Operation(const char *key, size_t nkey) : CommandType(key, nkey) {}
    Operation(const std::string& key) : CommandType(key) {}

    Operation<StoreCommand,R>(const std::string& k, const std::string& v,
        lcb_storage_t mode = LCB_SET) : StoreCommand(k, v, mode) {
    }
    Operation<StoreCommand,R>(const char *k, const char *v, lcb_storage_t mode = LCB_SET) :
            StoreCommand(k, v, mode) {
    }

    //! Get the response
    //! @return a reference to the inner response object
    R response() { return res; }
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
    inline R run(Client& client);
protected:
    friend class BatchContext;
    inline Status schedule_cli(Client&);
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
typedef Operation<UnlockCommand, UnlockResponse> UnlockOperation;

#define LCB_CXX_STORE_CTORS(cName, mType) \
    cName(const std::string k, const std::string& v) : StoreOperation(k, v, mType) {} \
    cName(const char *k, const char *v) : StoreOperation(k, v, mType) {} \
    cName(const char *key, size_t nkey, const char *value, size_t nvalue) : StoreOperation(mType) { \
        this->key(key, nkey); \
        this->value(value, nvalue); \
    }

lcb_t BatchContext::handle() const { return parent.handle(); }
} // namespace Couchbase

#include <libcouchbase/couchbase++/mctx.inl.h>
#include <libcouchbase/couchbase++/operations.inl.h>

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
#include <libcouchbase/couchbase++/endure.h>
#include <libcouchbase/couchbase++/client.inl.h>
#include <libcouchbase/couchbase++/batch.inl.h>

#endif
