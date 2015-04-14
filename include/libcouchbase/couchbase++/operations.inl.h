// Inline definitions for lcb++
namespace Couchbase {
#define LCB_CXX_IMPL_SCHEDFUNC(tinst, lcbfunc) \
template<>inline Status tinst::schedule_lcb(lcb_t instance) { \
    return lcbfunc(instance, &res, &static_cast<tinst::CommandType>(*this)); \
}

LCB_CXX_IMPL_SCHEDFUNC(GetOperation, lcb_get3)
LCB_CXX_IMPL_SCHEDFUNC(RemoveOperation, lcb_remove3)
LCB_CXX_IMPL_SCHEDFUNC(TouchOperation, lcb_touch3)
LCB_CXX_IMPL_SCHEDFUNC(CounterOperation, lcb_counter3)
LCB_CXX_IMPL_SCHEDFUNC(StoreOperation, lcb_store3)
LCB_CXX_IMPL_SCHEDFUNC(UnlockOperation, lcb_unlock3)

template <> inline
Status ObserveOperation::schedule_lcb(lcb_t instance) {
    lcb_MULTICMD_CTX *mctx = lcb_observe3_ctxnew(instance);
    if (!mctx) { return LCB_CLIENT_ENOMEM; }

    const lcb_CMDOBSERVE *obscmd = &static_cast<ObserveCommand>(*this);
    Status st = mctx->addcmd(mctx, reinterpret_cast<const lcb_CMDBASE*>(obscmd));
    if (st.success()) { return mctx->done(mctx, &res); }
    else { mctx->fail(mctx); return st; }
}

template <typename C, typename R> inline R
Operation<C,R>::run(Client& client) {
    BatchContext b(client);
    Status st = schedule(b);
    if (!st) {
        return R::setcode(res, st);
    }
    if (st) {
        b.submit();
        client.wait();
        return res;
    } else {
        b.bail();
        return R::setcode(res, st);
    }
}

GetResponse::GetResponse() : Response() {
    u.resp.bufh = NULL;
    u.resp.value = NULL;
    u.resp.nvalue = 0;
}

GetResponse& GetResponse::operator=(const GetResponse& other) {
    clear();
    assign_first(other);
    return *this;
}

bool
GetResponse::has_shared_buffer() const {
    return u.resp.bufh != NULL && u.resp.value != NULL;
}
bool
GetResponse::has_alloc_buffer() const {
    return u.resp.bufh == NULL && u.resp.value != NULL;
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
