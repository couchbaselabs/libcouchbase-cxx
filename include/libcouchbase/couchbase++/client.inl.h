#ifndef LCB_PLUSPLUS_H
#error "include <libcouchbase/couchbase++.h> first"
#endif

namespace Couchbase {

namespace Internal {
extern "C" {
static inline void cbwrap(lcb_t instance, int cbtype, const lcb_RESPBASE *res)
{
    Client *h = reinterpret_cast<Client*>((void*)lcb_get_cookie(instance));
    h->_dispatch(cbtype, res);
}
}
}

void
Client::_dispatch(int cbtype, const lcb_RESPBASE *r)
{
    auto *bresp = reinterpret_cast<Handler*>(r->cookie);
    bresp->handle_response(*this, cbtype, r);
    if (bresp->done()) {
        remaining--;
        breakout();
    }
}

Client::Client(const std::string& connstr, const std::string& passwd, const std::string& username)
: remaining(0), m_duropts(PersistTo::NONE, ReplicateTo::NONE)
{
    lcb_create_st cropts;
    cropts.version = 3;
    cropts.v.v3.connstr = connstr.c_str();
    if (!passwd.empty()) {
        cropts.v.v3.passwd = passwd.c_str();
    }
    if (!username.empty()) {
        cropts.v.v3.username = username.c_str();
    }
    Status rv = lcb_create(&m_instance, &cropts);
    if (rv != LCB_SUCCESS) {
        throw rv;
    }
    lcb_set_cookie(m_instance, this);
}

Client::~Client()
{
    lcb_destroy(m_instance);
}

void
Client::wait()
{
    lcb_wait3(m_instance, LCB_WAIT_NOCHECK);
}

Status
Client::connect()
{
    Status ret = lcb_connect(m_instance);
    if (!ret.success()) {
        return ret;
    }

    wait();
    ret = lcb_get_bootstrap_status(m_instance);
    if (ret.success()) {
        lcb_install_callback3(m_instance, LCB_CALLBACK_DEFAULT, Internal::cbwrap);
    }
    return ret;
}

template <typename T> Status
Client::schedule(const Command<T>& command, Handler *handler) {
    return command.scheduler()(handle(), handler, &command);
}

template <typename T, typename R> Status
Client::run(const Command<T>& command, Response<R>& response) {
    enter();
    Status s = schedule(command, &response);
    if (!s) {
        fail();
    } else {
        leave();
        wait();
    }

    response.set_key(command.keybuf(), command.keylen());
    return s;
}

GetResponse
Client::get(const GetCommand& cmd) {
    GetResponse resp;
    run(cmd, resp);
    return resp;
}

TouchResponse
Client::touch(const TouchCommand& cmd) {
    TouchResponse resp;
    run(cmd, resp);
    return resp;
}

template <lcb_storage_t T> StoreResponse
Client::store(const StoreCommand<T>& cmd) {
    StoreResponse resp;
    run(cmd, resp);
    return resp;
}

RemoveResponse
Client::remove(const RemoveCommand& cmd) {
    RemoveResponse resp;
    run(cmd, resp);
    return resp;
}

CounterResponse
Client::counter(const CounterCommand& cmd) {
    CounterResponse resp;
    run(cmd, resp);
    return resp;
}

StatsResponse
Client::stats(const std::string& key) {
    StatsCommand cmd(key);
    StatsResponse resp;
    run(cmd, resp);
    return resp;
}

UnlockResponse
Client::unlock(const UnlockCommand& cmd) {
    UnlockResponse resp;
    run(cmd, resp);
    return resp;
}

EndureResponse
Client::endure(const EndureCommand& cmd, const DurabilityOptions *options)
{
    EndureResponse resp;
    Status st;
    if (options == NULL) {
        options = const_cast<const DurabilityOptions*>(&m_duropts);
    }
    if (!options->enabled()) {
        return EndureResponse::setcode(resp, Status(LCB_EINVAL));
    }
    EndureContext ctx(*this, *options, &resp, st);
    if (!st) {
        return EndureResponse::setcode(resp, st);
    }
    st = ctx.add(cmd);
    if (!st) {
        return EndureResponse::setcode(resp, st);
    }
    st = ctx.submit();
    if (!st) {
        return EndureResponse::setcode(resp, st);
    }
    wait();
    return resp;
}

Status
Client::mctx_endure(const DurabilityOptions& opts,
    Handler *handler, Internal::MultiDurContext& out)
{
    lcb_error_t rv = LCB_SUCCESS;
    lcb_MULTICMD_CTX *mctx = lcb_endure3_ctxnew(m_instance, &opts, &rv);
    if (mctx == NULL) {
    } else {
        out = Internal::MultiDurContext(mctx, handler, this);
    }
    return rv;
}

Status
Client::mctx_observe(Handler *handler, Internal::MultiObsContext& out) {
    lcb_MULTICMD_CTX *mctx = lcb_observe3_ctxnew(m_instance);
    out = Internal::MultiObsContext(mctx, handler, this);
    return Status();
}

void
GetResponse::handle_response(Client&, int, const lcb_RESPBASE *resp)
{
    u.resp = *(lcb_RESPGET *)resp;
    if (status().success()) {
        if (u.resp.bufh) {
            lcb_backbuf_ref((lcb_BACKBUF) u.resp.bufh);
        } else if (u.resp.nvalue) {
            char *tmp = new char[u.resp.nvalue + sizeof(size_t)];
            u.resp.value = tmp;
            size_t rc = 1;
            memcpy(vbuf_refcnt(), &rc, sizeof rc);
        }
    } else {
        u.resp.bufh = NULL;
        u.resp.value = NULL;
    }
}

inline void
GetResponse::clear()
{
    if (has_shared_buffer()) {
        lcb_backbuf_unref((lcb_BACKBUF)u.resp.bufh);
    } else if (has_alloc_buffer()) {
        size_t rc;
        memcpy(&rc, vbuf_refcnt(), sizeof rc);
        if (rc == 1) {
            delete[] (char *)u.resp.value;
        } else {
            rc--;
            memcpy(vbuf_refcnt(), &rc, sizeof rc);
        }
    }
    u.resp.value = NULL;
    u.resp.nvalue = 0;
    u.resp.bufh = NULL;
}

void GetResponse::assign_first(const GetResponse& other) {
    u.resp = other.u.resp;
    if (has_shared_buffer()) {
        lcb_backbuf_ref((lcb_BACKBUF)u.resp.bufh);
    } else if (has_alloc_buffer()) {
        size_t rc;
        memcpy(&rc, vbuf_refcnt(), sizeof rc);
        rc ++;
        memcpy(vbuf_refcnt(), &rc, sizeof rc);
    }
}

void
StatsResponse::handle_response(Client& c, int t, const lcb_RESPBASE *resp)
{
    if (!initialized) {
        Response::handle_response(c, t, resp);
    }
    if (resp->rc != LCB_SUCCESS) {
        if (u.base.rc == LCB_SUCCESS) {
            u.base.rc = resp->rc;
        }
        return;
    }
    if (resp->rflags & LCB_RESP_F_FINAL) {
        m_done = true;
        return;
    }

    auto rs = reinterpret_cast<const lcb_RESPSTATS*>(resp);
    std::string server = rs->server;
    std::string key((const char*)rs->key, rs->nkey);
    std::string value((const char *)rs->value, rs->nvalue);
    auto iter = stats.find(key);
    if (iter == stats.end()) {
        stats[key] = std::map<std::string,std::string>();
    }
    stats[key][server] = value;
}

void
ObserveResponse::handle_response(Client&, int, const lcb_RESPBASE *res)
{
    if (res->rflags & LCB_RESP_F_FINAL) {
        return;
    }
    if (!initialized) {
        initialized = true;
        u.base = *res;
    }
    if (res->rc != LCB_SUCCESS && u.base.rc == LCB_SUCCESS) {
        u.base.rc = res->rc;
    }

    if (res->rc == LCB_SUCCESS) {
        ServerReply r;
        const lcb_RESPOBSERVE *ro = (const lcb_RESPOBSERVE *)res;
        r.cas = res->cas;
        r.status = ro->status;
        r.master = ro->ismaster;
        sinfo.push_back(r);
    }
}

const ObserveResponse::ServerReply&
ObserveResponse::master_reply() const
{
    static ServerReply dummy;
    for (auto ii = sinfo.begin(); ii != sinfo.end(); ii++) {
        if (ii->master) {
            return *ii;
        }
    }
    return dummy;
}

GetResponse::GetResponse() : Response() {
    u.resp.bufh = NULL;
    u.resp.value = NULL;
    u.resp.nvalue = 0;
}

GetResponse&
GetResponse::operator=(const GetResponse& other) {
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

// Durability response stuff
template <typename T> void
DurableResponse<T>::handle_response(Client& client, int cbtype, const lcb_RESPBASE *rb)
{
    if (m_state == State::STORE) {
        // Meaning we're in the initial operation phase:
        m_op.handle_response(client, cbtype, rb);
        assert(m_op.done());
        Status status;
        EndureContext dctx(client, m_dur, this, status);
        if (!status) {
            dur_bail(status);
            return;
        }
        EndureCommand dCmd(static_cast<const char*>(rb->key), rb->nkey, rb->cas);
        status = dctx.add(dCmd);
        if (!status) {
            dur_bail(status);
            return;
        }
        client.enter();
        status = dctx.submit();
        if (!status) {
            client.fail();
            dur_bail(status);
            return;
        }
        client.leave();
        m_state = State::SUBMIT;
    } else {
        m_dur.handle_response(client, cbtype, rb);
    }
}

template <typename T> bool
DurableResponse<T>::done() const
{
    if (m_state == State::DONE) {
        return m_dur.done();
    } else if (m_state == State::STATE_ERROR) {
        return true;
    } else {
        return false;
    }
}

template <typename T> void
DurableResponse<T>::dur_bail(Status& st)
{
    EndureResponse::setcode(m_dur, st);
    m_state = State::STATE_ERROR;
}

} // namespace Couchbase
