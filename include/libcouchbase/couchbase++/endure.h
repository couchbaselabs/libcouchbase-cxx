#ifndef LCB_PLUSPLUS_H
#error "include <libcouchbase/couchbase++.h> first"
#endif

#ifndef LCB_PLUSPLUS_ENDURE_H
#define LCB_PLUSPLUS_ENDURE_H

namespace Couchbase {

class DurabilityContext;

//! Options for durability constraints
class DurabilityOptions : public lcb_durability_opts_t {
public:
    //! Construct a durability options object
    //! @param persist_to @see #persist_to()
    //! @param replicate_to @see #replicate_to()
    //! @param cap_max @see cap_max
    DurabilityOptions(size_t persist_to, size_t replicate_to, bool cap_max = true) {
        memset(this, 0, sizeof *this);
        v.v0.persist_to = persist_to;
        v.v0.replicate_to = replicate_to;
        v.v0.cap_max = cap_max;
    }
    //! Wait for persistence to this many nodes
    //! @param n the number of nodes to wait for
    void persist_to(size_t n) { v.v0.persist_to = n; }

    //! Wait for replication to this many replicas
    //! @param n the number of replica nodes to wait to
    void replicate_to(size_t n) { v.v0.replicate_to = n; }

    //! Whether the client should try the maximum number of nodes, in case
    //! the number of nodes/replicas specified by #persist_to() and
    //! #replicate_to() currently exceed the cluster topology
    //! @param enabled whether to enable this behavioe
    void cap_max(bool enabled) { v.v0.cap_max = enabled; }
};

//! Response received for durability operations
class EndureResponse : public Response<lcb_RESPENDURE> {
public:
    //! @private
    void init(const lcb_RESPBASE *res) override {
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

//! This class is used to perform a single durability operation
class DurabilityOperation : public Operation<Command<lcb_CMDENDURE>, EndureResponse> {
public:
    //! Create the operation using the result of a previous operation
    //! @param op a @ref StoreOperation
    template <typename T> DurabilityOperation(const T& op) : Operation() {
        key(op.get_keybuf(), op.get_keylen());
        cas(op.const_response().cas());
    }

    //! Create the operation using a raw key and CAS value
    //! @param key The key to check
    //! @param cas the CAS to check
    DurabilityOperation(const char *key, uint64_t cas) : Operation() {
        this->key(key);
        this->cas(cas);
    }

    DurabilityOperation(const char *k, size_t n, uint64_t c) : Operation() {
        this->key(k, n);
        this->cas(c);
    }

    //! Run this operation and wait for its result.
    //! @param h the client
    inline EndureResponse& run(Client& h);

    //! Schedule this operation on a context to allow for operation batching
    //! @param ctx the context (should be a @ref DurabilityContext)
    inline Status schedule(DurabilityContext& ctx);
};

//! This class is similar to the @ref BatchContext (though not a subclass).
//! Note that currently a DurabilityContext is mutually exclusive with a
//! BatchContext
class DurabilityContext {
public:
    //! Create a new DurabilityContext
    //! @param c the client
    //! @param options options for the durability operation(s)
    //! @param status. Check this after the call to the constructor for any
    //!        errors. If status is not successful then this object must not
    //!        be used further.
    DurabilityContext(Client& c, const DurabilityOptions& options, Status& status) : h(c){
        lcb_error_t rc = LCB_SUCCESS;
        lcb_sched_enter(h.handle());
        mctx = lcb_endure3_ctxnew(h.handle(), &options, &rc);
        status = rc;
        nremaining = 0;
    }

    //! Clear any pending operations. @see BatchContext#bail()
    void bail() {
        nremaining = 0;
        if (mctx != NULL) {
            mctx->fail(mctx);
            mctx = NULL;
        }
    }

    //! Adds an operation to this context
    //! @param op the operation
    //! @return status code indicating if the operation was successfully
    //!         added.
    Status addop(const DurabilityOperation& op) {
        Status st = mctx->addcmd(mctx, op.as_basecmd());
        if (st) {
            nremaining++;
            resps[std::string(op.get_keybuf(), op.get_keylen())] =
                    const_cast<EndureResponse*>(&op.const_response());
        }
        return st;
    }

    //! Submit this context to be executed during the next call to
    //! Client#wait().
    //! @return a status code which will contain any errors if the context
    //!         could not be scheduled.
    Status submit() {
        Status st = mctx->done(mctx, this);
        mctx = NULL;
        if (st) {
            lcb_sched_leave(h.handle());
        }
        return st;
    }

    //! @private
    EndureResponse *find_response(const lcb_RESPBASE *rb) {
        std::string tmp((const char*)rb->key, rb->nkey);
        return resps[tmp];
    }

    ~DurabilityContext() { bail(); }
private:
    Client& h;
    std::map<std::string, EndureResponse*> resps;
    size_t nremaining;
    lcb_MULTICMD_CTX *mctx;
};

EndureResponse&
DurabilityOperation::run(Client& h)
{
    Status st;
    DurabilityContext ctx(h, DurabilityOptions(-1,-1), st);
    if (!st) {
        return EndureResponse::setcode(res, st);
    }
    st = ctx.addop(*this);
    if (!st) {
        return EndureResponse::setcode(res, st);
    }
    st = ctx.submit();
    if (!st) {
        return EndureResponse::setcode(res, st);
    }
    h.wait();
    return res;
}
}

#endif
