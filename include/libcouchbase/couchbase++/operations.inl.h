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
    if (st) {
        b.submit();
        client.wait();
        return res;
    } else {
        b.bail();
        return R::setcode(res, st);
    }
}

} // namespace
