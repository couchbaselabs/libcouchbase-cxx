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
#include <list>
#include <iostream>
#include <functional>
#include <memory>
#include <libcouchbase/couchbase++/forward.h>
#include <libcouchbase/couchbase++/status.h>

namespace Couchbase {

typedef lcb_storage_t StoreMode;

// Simple buffer class
class Buffer {
public:
    Buffer(const Buffer& other) : m_data(other.m_data), m_length(other.m_length) {}
    Buffer(const void *buf, size_t len) : m_data(buf), m_length(len) {}
    Buffer(){}

    const char *data() const { return reinterpret_cast<const char*>(m_data); }
    size_t length() const { return m_length; }
    size_t size() const { return length(); }
    bool empty() const { return length() == 0; }
    std::string to_string() const {
        return empty() ? "" : std::string(data(), length());
    }
    operator std::string() const {
        return to_string();
    }

    typedef const char* const_iterator;

    const_iterator begin() const {
        return reinterpret_cast<const char*>(m_data);
    }

    const_iterator end() const {
        return m_length == 0 ?
                begin() : reinterpret_cast<const char*>(m_data) + m_length;
    }

private:
    const void *m_data = NULL;
    size_t m_length = 0;
};
}

namespace std {
inline ostream& operator<<(ostream& os, const Couchbase::Buffer& obj) {
    return os.write(obj.data(), obj.size());
}
}

namespace Couchbase {

//! Base class for all commands.
template <typename T>
class Command {
public:
    typedef typename T::CType LcbType;
    typedef T Info; // Information about the type

    Command() { memset(&m_cmd, 0, sizeof m_cmd); }

    //! Set the key for the command
    //! @param buf the buffer for the key
    //! @param len the size of the key
    //! @note The buffer must remain valid until the operation has been
    //! scheduled.
    void key(const char *buf, size_t len) { LCB_CMD_SET_KEY(&m_cmd, buf, len); }
    void key(const char *buf) { key(buf, strlen(buf)); }
    void key(const std::string& s) { key(s.c_str(), s.size()); }
    void key(const Buffer& buf) { key(buf.data(), buf.size()); }
    Buffer key() const { return Buffer(keybuf(), keylen()); }

    const char *keybuf() const { return (const char *)m_cmd.key.contig.bytes; }
    size_t keylen() const { return m_cmd.key.contig.nbytes; }

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
    const LcbType* operator&() const { return &m_cmd; }

    //! Actual libcouchbase function used to schedule the command
    typedef lcb_error_t (*Scheduler)(lcb_t, const void*, const LcbType*);
    inline Scheduler scheduler() const;
protected:
    LcbType m_cmd;
};

#define LCB_CXX_DECLSCHED(cmdname, schedname) \
template<> inline Command<cmdname>::Scheduler \
Command<cmdname>::scheduler() const { return schedname; }

LCB_CXX_DECLSCHED(OpInfo::Get, lcb_get3)
LCB_CXX_DECLSCHED(OpInfo::Store, lcb_store3)
LCB_CXX_DECLSCHED(OpInfo::Touch, lcb_touch3)
LCB_CXX_DECLSCHED(OpInfo::Remove, lcb_remove3)
LCB_CXX_DECLSCHED(OpInfo::Stats, lcb_stats3)
LCB_CXX_DECLSCHED(OpInfo::Unlock, lcb_unlock3)
LCB_CXX_DECLSCHED(OpInfo::Counter, lcb_counter3)

#define LCB_CXX_CMD_CTOR(name) \
    name() : Command() {} \
    name(const char *k) : Command() {key(k);} \
    name(const char *k, size_t n) : Command() {key(k,n);} \
    name(const std::string& s) : Command() {key(s);}

//! @class GetCommand
//! @brief Command structure for retrieving items
class GetCommand : public Command<OpInfo::Get> {
public:
    LCB_CXX_CMD_CTOR(GetCommand)
    //! @brief Make this get operation be a get-and-lock operation
    //! Set this value to the time the lock will be held. Operations to mutate
    //! the key while the lock is still held on the server will fail. You may
    //! unlock the item using @ref UnlockCommand.
    void locktime(unsigned n) { expiry(n); m_cmd.lock = 1; }
    Scheduler scheduler() const { return lcb_get3; }
};

//! @brief Command structure for mutating/storing items
template <StoreMode M>
class StoreCommand : public Command<OpInfo::Store> {
public:
    StoreCommand() : Command() {
        mode(M);
    }
    StoreCommand(const std::string& key, const std::string& value) : Command() {
        this->key(key); this->value(value); this->mode(M);
    }
    StoreCommand(const char *key, const char *value) : Command() {
        this->key(key); this->value(value); this->mode(M);
    }
    StoreCommand(const char *key, size_t nkey, const char *value, size_t nvalue)
    :Command() {
        this->key(key, nkey); this->value(value, nvalue);
    }

    //! @brief Explicitly set the mutation type
    //! @param op the mutation type.
    void mode(StoreMode op) { m_cmd.operation = op; }

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

typedef StoreCommand<LCB_SET> UpsertCommand;
typedef StoreCommand<LCB_ADD> InsertCommand;
typedef StoreCommand<LCB_REPLACE> ReplaceCommand;
typedef StoreCommand<LCB_APPEND> AppendCommand;
typedef StoreCommand<LCB_PREPEND> PrependCommand;

//! Command structure for atomic counter operations.
//! This command treats a value as a number (the value must be formatted as a
//! series of ASCII digis).
class CounterCommand : public Command<OpInfo::Counter> {
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
class RemoveCommand : public Command<OpInfo::Remove> {
public:
    LCB_CXX_CMD_CTOR(RemoveCommand)
};

//! Command structure for retrieving statistics from the cluster
class StatsCommand : public Command<OpInfo::Stats> {
public:
    LCB_CXX_CMD_CTOR(StatsCommand)
};

//! Command to update expiration times of items
class TouchCommand : public Command<OpInfo::Store> {
public:
    LCB_CXX_CMD_CTOR(TouchCommand)
};

//! Command to unlock previously locked items
class UnlockCommand : public Command<OpInfo::Unlock> {
public:
    LCB_CXX_CMD_CTOR(UnlockCommand)
};

//! Command to retrieve persistence/replication metadata for an item
class ObserveCommand : public Command<OpInfo::Observe> {
public:
    LCB_CXX_CMD_CTOR(ObserveCommand)
    void master_only(bool val=true) {
        if (val) {
            m_cmd.cmdflags |= static_cast<uint32_t>(LCB_CMDOBSERVE_F_MASTER_ONLY);
        } else {
            m_cmd.cmdflags &= ~static_cast<uint32_t>(LCB_CMDOBSERVE_F_MASTER_ONLY);
        }
    }
};

//! Command to ensure persistence/replication of an item to a number of nodes.
//! @see DurabilityOptions
class EndureCommand : public Command<OpInfo::Endure> {
public:
    EndureCommand(const char *key, uint64_t cas) : Command() {
        this->key(key); this->cas(cas);
    }
    EndureCommand(const char *key, size_t nkey, uint64_t cas) : Command() {
        this->key(key, nkey); this->cas(cas);
    }
    EndureCommand(const std::string& key, uint64_t cas) : Command() {
        this->key(key); this->cas(cas);
    }
    const DurabilityOptions* options() const { return m_options; }
    void options(const DurabilityOptions *o) { m_options = o; }

private:
    const DurabilityOptions *m_options = NULL;
};

//! Persistence setting
enum class PersistTo : lcb_U16 {
    NONE = 0, //!< Dont't wait for persistence
    MASTER = 1, //!< Only wait for persistence to master node
    TWO = 2, //!< Wait for persistence to master and one replicas
    THREE = 3, //!< Wait for persistence to master and two replicas
    FOUR = 4 //!< Wait for persistence to master and three replicas
};

//! Replication settings
enum class ReplicateTo : lcb_U16 {
    NONE = 0, //!< Don't wait for replication
    ONE = 1, //!< Wait for replication to one replica
    TWO = 2, //!< Wait for replication to two replicas
    THREE = 3 //!< Wait for replication to three replicas
};

//! Options for durability constraints
class DurabilityOptions : public lcb_durability_opts_t {
public:
    //! Construct a durability options object
    //! @param persist_to @see #persist_to()
    //! @param replicate_to @see #replicate_to()
    //! @param cap_max @see cap_max
    DurabilityOptions(
        PersistTo n_persist = PersistTo::NONE,
        ReplicateTo n_replicate = ReplicateTo::NONE,
        bool cap_max = true) {

        memset(static_cast<lcb_durability_opts_t*>(this),
            0, sizeof (lcb_durability_opts_t));

        persist_to(n_persist);
        replicate_to(n_replicate);
        v.v0.cap_max = cap_max;
    }
    //! Wait for persistence to this many nodes
    //! @param n the number of nodes to wait for
    void persist_to(PersistTo n) { v.v0.persist_to = static_cast<uint16_t>(n); }

    //! Wait for replication to this many replicas
    //! @param n the number of replica nodes to wait to
    void replicate_to(ReplicateTo n) { v.v0.replicate_to = static_cast<uint16_t>(n); }

    //! Whether the client should try the maximum number of nodes, in case
    //! the number of nodes/replicas specified by #persist_to() and
    //! #replicate_to() currently exceed the cluster topology
    //! @param enabled whether to enable this behavioe
    void cap_max(bool enabled) { v.v0.cap_max = enabled; }

    bool enabled() const {
        return v.v0.persist_to || v.v0.replicate_to;
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
template <typename T=OpInfo::Base>
class Response : public Handler {
public:
    typedef typename T::RType LcbType;

    //! Get status
    //! @return the status of the operation.
    Status status() const { return u.base.rc; }

    //! Determine if response is final
    //! @return true if this is the final response, or if there are more
    //!         responses for the given command.
    bool final() const { return u.base.rflags & LCB_RESP_F_FINAL; }

    //! Get the CAS for the operation. Only valid if the operation succeeded,
    //! @return the CAS.
    uint64_t cas() const { return u.base.cas; }

    Response() {}
    virtual ~Response() {}

    //! @private
    template <class D> static D& setcode(D& r, const Status& c) {
        r.u.base.rc = c;
        return r;
    }

    //! @private
    virtual void handle_response(Client&, int, const lcb_RESPBASE *res) override {
        u.resp = *reinterpret_cast<const LcbType*>(res);
    }

    virtual bool done() const override {
        return true;
    }

    //! Get the key for the response.
    //! @return the key
    //! @note The key contains the pointer to the key passed during request
    //!       time. If the key passed to the original command is no longer
    //!       valid, this this function will crash.
    std::string key() const { return std::string(keybuf(), keylen()); }
    const char *keybuf() const { return static_cast<const char*>(u.base.key); }
    size_t keylen() const { return u.base.nkey; }

    void set_key(const lcb_RESPBASE *rb) {
        set_key(static_cast<const char*>(rb->key), rb->nkey);
    }

protected:
    union { lcb_RESPBASE base; LcbType resp; } u;
private:
    void set_key(const char *key, size_t nkey) {
        u.base.key = key;
        u.base.nkey = nkey;
    }
    friend class Client;
};

namespace Internal {
typedef Response<OpInfo::Base> BaseResponse;
}

typedef Response<OpInfo::Store> StoreResponse;
typedef Response<OpInfo::Remove> RemoveResponse;
typedef Response<OpInfo::Touch> TouchResponse;
typedef Response<OpInfo::Unlock> UnlockResponse;

//! @brief Response for @ref GetCommand requests
class GetResponse : public Response<OpInfo::Get> {
public:
    inline GetResponse();
    GetResponse(const GetResponse& other) { assign_first(other); }
    inline GetResponse& operator=(const GetResponse&);

    ~GetResponse() { clear(); }

    //! @brief Release memory used by the response value
    inline void clear();

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

    Buffer value() const { return Buffer(valuebuf(), valuesize()); }

    //! Get the flags of the item. See StoreCommand::itemflags
    uint32_t valueflags() const { return u.resp.itmflags; }
    uint32_t itemflags() const { return valueflags(); }

    //! @private
    inline void handle_response(Client&, int, const lcb_RESPBASE *) override;

private:
    friend class Client;
    friend class ViewRow;
    inline void assign_first(const GetResponse& other);

    inline bool has_shared_buffer() const;
    inline bool has_alloc_buffer() const;
    inline char *vbuf_refcnt();
};

class StatsResponse : public Response<OpInfo::Stats> {
public:
    StatsResponse() : Response() { }
    inline void handle_response(Client&, int, const lcb_RESPBASE *resp) override;
    bool done() const override { return m_done; }
    std::map<std::string,std::map<std::string,std::string> > stats;
private:
    bool initialized = false;
    bool m_done = false;
};

class CounterResponse : public Response<OpInfo::Counter> {
public:
    void handle_response(Client&, int, const lcb_RESPBASE *res) override {
        u.resp = *(lcb_RESPCOUNTER *)res;
    }

    //! Get the current counter value
    //! @returns the current counter value. This is the value after the
    //!          counter operation. Only valid if the operation succeeded.
    uint64_t value() const { return u.resp.value; }
};

class ObserveResponse : public Response<OpInfo::Observe> {
public:
    struct ServerReply {
        uint64_t cas = 0;
        bool master = false;
        uint8_t status = 0;
        ServerReply() {}
        bool exists() const { return (status == LCB_OBSERVE_FOUND) || persisted(); }
        bool persisted() const { return status & (int)LCB_OBSERVE_PERSISTED; }
    };

    ObserveResponse() : Response() { }
    inline void handle_response(Client&, int, const lcb_RESPBASE *res) override;
    inline const ServerReply& master_reply() const;
    const std::vector<ServerReply>& all_replies() const { return sinfo; }
private:
    bool initialized = false;
    std::vector<ServerReply> sinfo;
};

//! Response received for durability operations
class EndureResponse : public Response<OpInfo::Endure> {
public:
    //! @private
    void handle_response(Client&, int, const lcb_RESPBASE *res) override {
        u.resp = *(lcb_RESPENDURE*)res;
    }

    //! Check if the item was persisted on the master node's storage
    //! @return true if the item was persisted to the master node
    bool on_master_storage() const { return u.resp.persisted_master ? true : false; }

    //! Check whether the item exists on the master at all. If this is false
    //! then the item was either modified or the cluster has failed over
    //! @return true if the item exists on the master; false otherwise
    bool on_master_ram() const { return u.resp.exists_master ? true : false; }

    //! Return the number of observe commands sent. This is mainly for
    //! informational/debugging purposes
    //! @return the number of `OBSERVE` commands sent to each server
    size_t probes() const { return u.resp.nresponses; }

    //! Get the number of nodes to which the item was actually persisted
    //! @return the number of nodes to which the mutation was persisted
    size_t persisted() const { return u.resp.npersisted; }

    //! Get the number of nodes to which the item was replicated
    //! @return the number of replicas containing this mutation.
    size_t replicate() const { return u.resp.nreplicated; }
};

//! Response object wrapping a durable operation.
template<typename T>
class DurableResponse : public Handler {
public:
    inline void handle_response(Client&, int, const lcb_RESPBASE*) override;
    inline bool done() const override;
    DurableResponse(const DurabilityOptions *options) : m_duropt(options){}
private:
    inline void dur_bail(Status&);
    const DurabilityOptions *m_duropt;
    T m_op;
    EndureResponse m_dur;

    enum class State { STORE = 0, SUBMIT, DONE, STATE_ERROR };
    State m_state = State::STORE;
};

namespace Internal {
template <typename T>
class MultiContextT {
public:
    inline MultiContextT();
    inline MultiContextT(lcb_MULTICMD_CTX*, Handler*, Client*);
    inline MultiContextT& operator=(MultiContextT&&);
    inline ~MultiContextT();
    inline Status add(const T*);
    inline Status done();
    inline void bail();
private:
    lcb_MULTICMD_CTX *m_inner;
    Handler *cookie;
    Client *client;
    MultiContextT(MultiContextT&) = delete;
    MultiContextT& operator=(MultiContextT&) = delete;
};
}

//! @brief A Context can be used to efficiently batch multiple operations.
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
class Context {
public:
    //! @brief Initialize the batch client.
    //! @param client the client to use.
    //! @warning Only one batch context can be active on a client at a given
    //!          time. In addition, when a batch context is active, higher
    //!          level calls on the client (such as Client::get()) cannot be
    //!          performed.
    inline Context(Client& client);
    inline Context(Context&&);
    inline Context& operator=(Context&&);
    inline ~Context();

    //! @brief Cancel any operations submitted in the current batch. This also
    //!        deactivates the batch. This is useful if scheduling several
    //!        dependent operations, where one of the operations fail.
    inline void bail();
    template <typename T> inline Status add(const Command<T>&, Handler *);

    //! @brief Submit all previously scheduled operations. These operations
    //!        will be performed when Client::wait() is called. This function
    //!        also deactivates the batch.
    inline void submit();

    //! @brief Re-activates the batch.
    inline void reset();
private:
    bool entered = false;
    size_t m_remaining = 0;
    Client& parent;
    Context(Context&) = delete;
    Context& operator=(Context&) = delete;
};

template <typename C, typename R>
class BatchCommand {
public:
    typedef std::list<R> RList;
    inline BatchCommand(Client&);
    inline Status add(const C& cmd);
    template <typename ...Params> Status add(Params... params) {
        return add(C(params...));
    }
    Context& context() { return m_ctx; }
    void submit() { m_ctx.submit(); }

    typename RList::const_iterator begin() const { return m_resplist.begin(); }
    typename RList::const_iterator end() const { return m_resplist.end(); }

protected:
    Context m_ctx; // Context (either implicit or explicit)
    RList m_resplist; // List of responses
};

template <typename C, typename R>
class CallbackCommand : public Handler {
public:
    typedef const std::function<void(R&)> CallbackType;
    inline CallbackCommand(Client&, CallbackType&);
    inline Status add(const C& cmd);
    template <typename ...Params> Status add(Params... params) {
        return add(C(params...));
    }
    void submit() { m_ctx.submit(); }
    void handle_response(Client&, int, const lcb_RESPBASE*) override;
    bool done() const override { return true; }
protected:
    CallbackType m_callback;
    Context m_ctx;
};

class EndureContext {
public:
    inline EndureContext(Client&, const DurabilityOptions&, Handler *h, Status &);
    inline Status add(const EndureCommand&);
    inline Status submit();
    inline void bail();
private:
    size_t m_remaining = 0;
    Internal::MultiDurContext m_ctx;
    Client& client;
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
    inline Client(const std::string& connstr = "couchbase://localhost/default", const std::string& passwd = "", const std::string& username = "");
    inline ~Client();

    inline GetResponse get(const GetCommand&);
    template <typename ...Params> GetResponse get(Params... params) {
        return get(GetCommand(params...));

    }

    template <lcb_storage_t T> inline StoreResponse store(const StoreCommand<T>&);

    template <typename ...Params> StoreResponse upsert(Params... params) {
        return store(UpsertCommand(params...));
    }
    template <typename ...Params> StoreResponse insert(Params... params) {
        return store(InsertCommand(params...));
    }
    template <typename ...Params> StoreResponse replace(Params... params) {
        return store(ReplaceCommand(params...));
    }

    inline TouchResponse touch(const TouchCommand&);

    inline RemoveResponse remove(const RemoveCommand&);
    template <typename ...Params> RemoveResponse remove(Params... params) {
        return remove(RemoveCommand(params...));
    }

    inline CounterResponse counter(const CounterCommand&);
    inline StatsResponse stats(const std::string& key);
    inline UnlockResponse unlock(const UnlockCommand& cmd);

    inline EndureResponse endure(const EndureCommand& cmd, const DurabilityOptions* options = NULL);
    inline EndureResponse endure(const EndureCommand& cmd, const DurabilityOptions& options) {
        return endure(cmd, &options);
    }

    template <typename T>
    inline Status schedule(const Command<T>&, Handler *);

    template <typename T, typename R>
    inline Status run(const Command<T>&, Response<R>&);

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
    inline void breakout(bool force=false) {
        if (!remaining || force) {
            lcb_breakout(m_instance);
        }
    }

    //! @private
    inline void _dispatch(int, const lcb_RESPBASE*);

    inline Status mctx_endure(const DurabilityOptions&, Handler*, Internal::MultiDurContext&);
    inline Status mctx_observe(Handler*, Internal::MultiObsContext&);

    void enter() { lcb_sched_enter(m_instance); }
    void leave() { lcb_sched_leave(m_instance); }
    void fail() { lcb_sched_fail(m_instance); }

private:
    friend class Context;
    friend class EndureContext;
    lcb_t m_instance;
    size_t remaining;
    DurabilityOptions m_duropts;
    Client(Client&) = delete;
};
} // namespace Couchbase

#include <libcouchbase/couchbase++/mctx.inl.h>
#include <libcouchbase/couchbase++/endure.h>
#include <libcouchbase/couchbase++/client.inl.h>
#include <libcouchbase/couchbase++/batch.inl.h>

#endif
