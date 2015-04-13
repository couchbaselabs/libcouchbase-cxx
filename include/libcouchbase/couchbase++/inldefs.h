// Inline definitions for lcb++
namespace Couchbase {
#define LCB_CXX_IMPL_SCHEDFUNC(tinst, lcbfunc) \
template<>inline Status tinst::scheduleLcb(lcb_t instance) { \
    return lcbfunc(instance, &res, &static_cast<tinst::_Cmdcls>(*this)); \
}

LCB_CXX_IMPL_SCHEDFUNC(GetOperation, lcb_get3)
LCB_CXX_IMPL_SCHEDFUNC(RemoveOperation, lcb_remove3)
LCB_CXX_IMPL_SCHEDFUNC(TouchOperation, lcb_touch3)
LCB_CXX_IMPL_SCHEDFUNC(CounterOperation, lcb_counter3)
LCB_CXX_IMPL_SCHEDFUNC(StoreOperation, lcb_store3)

template <> inline
Operation<StoreCommand,Response>::Operation(lcb_storage_t op) : StoreCommand(op) {
}

template <> inline
Status ObserveOperation::scheduleLcb(lcb_t instance) {
    lcb_MULTICMD_CTX *mctx = lcb_observe3_ctxnew(instance);
    if (!mctx) { return LCB_CLIENT_ENOMEM; }

    const lcb_CMDOBSERVE *obscmd = &static_cast<ObserveCommand>(*this);
    Status st = mctx->addcmd(mctx, reinterpret_cast<const lcb_CMDBASE*>(obscmd));
    if (st.success()) { return mctx->done(mctx, &res); }
    else { mctx->fail(mctx); return st; }
}

template <typename A, typename B> inline B
Operation<A,B>::run(Client& client) {
    BatchContext b(client);
    Status st = schedule(b);
    if (!st) {
        return Response::setcode(res, st);
    }
    if (st) {
        b.submit();
        client.wait();
        return res;
    } else {
        b.bail();
        return Response::setcode(res, st);
    }
}

GetResponse::GetResponse() : Response() {
    u.get.bufh = NULL;
    u.get.value = NULL;
    u.get.nvalue = 0;
}

GetResponse& GetResponse::operator=(const GetResponse& other) {
    clear();
    assign_first(other);
    return *this;
}

bool
GetResponse::hasSharedBuffer() const {
    return u.get.bufh != NULL && u.get.value != NULL;
}
bool
GetResponse::hasAllocBuffer() const {
    return u.get.bufh == NULL && u.get.value != NULL;
}
char *
GetResponse::vbuf_refcnt() {
    return const_cast<char*>(valuebuf()) + valuesize();
}

void
GetResponse::value(std::string& s) const {
    if (status()) {
        s.assign(valuebuf(), valuesize());
    }
}

void
GetResponse::value(std::vector<char>& v) const {
    if (status()) {
        v.insert(v.end(), valuebuf(), valuebuf()+valuesize());
    }
}

} // namespace
