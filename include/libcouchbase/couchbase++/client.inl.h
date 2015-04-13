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
    Response *resp;

    if (cbtype == LCB_CALLBACK_ENDURE) {
        if (r->rflags & LCB_RESP_F_FINAL) {
            remaining--;
            breakout();
        } else {
            DurabilityContext *ctx = reinterpret_cast<DurabilityContext*>(r->cookie);
            resp = ctx->find_response(r);
            resp->init(r);
        }
        return;
    }

    resp = reinterpret_cast<Response*>(r->cookie);
    resp->init(r);
    if (resp->done()) {
        remaining--;
        breakout();
    }
}

Client::Client(const std::string& connstr, const std::string& passwd) : remaining(0)
{
    lcb_create_st cropts;
    cropts.version = 3;
    cropts.v.v3.connstr = connstr.c_str();
    if (!passwd.empty()) {
        cropts.v.v3.passwd = passwd.c_str();
    }
    Status rv = lcb_create(&instance, &cropts);
    if (rv != LCB_SUCCESS) {
        throw rv;
    }
    lcb_set_cookie(instance, this);
}

Client::~Client()
{
    lcb_destroy(instance);
}

void
Client::wait()
{
    lcb_wait3(instance, LCB_WAIT_NOCHECK);
}

Status
Client::connect()
{
    Status ret = lcb_connect(instance);
    if (!ret.success()) {
        return ret;
    }

    wait();
    ret = lcb_get_bootstrap_status(instance);
    if (ret.success()) {
        lcb_install_callback3(instance, LCB_CALLBACK_DEFAULT, Internal::cbwrap);
    }
    return ret;
}

GetResponse
Client::get(const GetCommand& cmd) {
    GetOperation gop(cmd);
    gop.run(*this);
    return gop.response();
}

TouchResponse
Client::touch(const TouchCommand& cmd) {
    TouchOperation top(cmd);
    top.run(*this);
    return top.response();
}

StoreResponse
Client::upsert(const StoreCommand& cmd) {
    StoreOperation sop(cmd);
    sop.mode(LCB_SET);
    sop.run(*this);
    return sop.response();
}

StoreResponse
Client::add(const StoreCommand& cmd) {
    StoreOperation sop(cmd);
    sop.mode(LCB_ADD);
    sop.run(*this);
    return sop.response();
}

StoreResponse
Client::replace(const StoreCommand& cmd) {
    StoreOperation sop(cmd);
    sop.mode(LCB_REPLACE);
    sop.run(*this);
    return sop.response();
}

RemoveResponse
Client::remove(const RemoveCommand& cmd) {
    RemoveOperation rop(cmd);
    rop.run(*this);
    return rop.response();
}

CounterResponse
Client::counter(const CounterCommand& cmd) {
    CounterOperation cop(cmd);
    cop.run(*this);
    return cop.response();
}

StatsResponse
Client::stats(const std::string& key) {
    StatsCommand cmd(key);
    StatsResponse res;
    lcb_sched_enter(instance);
    Status rc = lcb_stats3(instance, &res, &cmd);
    if (!rc) {
        return Response::setcode(res, rc);
    } else {
        lcb_sched_leave(instance);
        wait();
        return res;
    }
}

UnlockResponse
Client::unlock(const UnlockCommand& cmd) {
    UnlockOperation uop(cmd);
    uop.run(*this);
    return uop.response();
}

void
GetResponse::init(const lcb_RESPBASE *resp)
{
    u.get = *(lcb_RESPGET *)resp;
    if (status().success()) {
        if (u.get.bufh) {
            lcb_backbuf_ref((lcb_BACKBUF) u.get.bufh);
        } else if (u.get.nvalue) {
            char *tmp = new char[u.get.nvalue + sizeof(size_t)];
            u.get.value = tmp;
            size_t rc = 1;
            memcpy(vbuf_refcnt(), &rc, sizeof rc);
        }
    } else {
        u.get.bufh = NULL;
        u.get.value = NULL;
    }
}

void
GetResponse::clear()
{
    if (hasSharedBuffer()) {
        lcb_backbuf_unref((lcb_BACKBUF)u.get.bufh);
    } else if (hasAllocBuffer()) {
        size_t rc;
        memcpy(&rc, vbuf_refcnt(), sizeof rc);
        if (rc == 1) {
            delete[] (char *)u.get.value;
        } else {
            rc--;
            memcpy(vbuf_refcnt(), &rc, sizeof rc);
        }
    }
    u.get.value = NULL;
    u.get.nvalue = 0;
    u.get.bufh = NULL;
}

void GetResponse::assign_first(const GetResponse& other) {
    u.get = other.u.get;
    if (hasSharedBuffer()) {
        lcb_backbuf_ref((lcb_BACKBUF)u.get.bufh);
    } else if (hasAllocBuffer()) {
        size_t rc;
        memcpy(&rc, vbuf_refcnt(), sizeof rc);
        rc ++;
        memcpy(vbuf_refcnt(), &rc, sizeof rc);
    }
}

void
StatsResponse::init(const lcb_RESPBASE *resp)
{
    if (!initialized) {
        Response::init(resp);
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

    lcb_RESPSTATS *rs = (lcb_RESPSTATS *)resp;
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
ObserveResponse::init(const lcb_RESPBASE *res)
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
} // namespace Couchbase
