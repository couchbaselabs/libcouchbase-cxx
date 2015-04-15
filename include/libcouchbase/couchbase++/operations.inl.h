// Inline definitions for lcb++
namespace Couchbase {

#define LCB_CXX_IMPL_SCHEDFUNC(tinst, schedfunc) \
template<>inline Status \
tinst::schedule_cli(Client& client) { \
    return client.schedfunc(&m_cmd, &res); \
}

LCB_CXX_IMPL_SCHEDFUNC(GetOperation, schedule_get)
LCB_CXX_IMPL_SCHEDFUNC(RemoveOperation, schedule_remove)
LCB_CXX_IMPL_SCHEDFUNC(TouchOperation, schedule_touch)
LCB_CXX_IMPL_SCHEDFUNC(CounterOperation, schedule_counter)
LCB_CXX_IMPL_SCHEDFUNC(StoreOperation, schedule_store)
LCB_CXX_IMPL_SCHEDFUNC(UnlockOperation, schedule_unlock)

template <> inline
Status ObserveOperation::schedule_cli(Client& client) {
    Internal::MultiObsContext obs;
    Status st = client.mctx_observe(&res, obs);
    if (!st) {
        return st;
    }
    st = obs.add(&m_cmd);
    if (!st) {
        return st;
    }
    return obs.done();
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
