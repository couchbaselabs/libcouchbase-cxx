#include <libcouchbase/couchbase++.h>
#include <libcouchbase/couchbase++/endure.h>
#include <stdio.h>
using namespace Couchbase;
using std::string;
using std::map;
using std::vector;

extern "C" {
static void cbwrap(lcb_t instance, int cbtype, const lcb_RESPBASE *res)
{
    Client *h = reinterpret_cast<Client*>((void*)lcb_get_cookie(instance));
    h->_dispatch(cbtype, res);
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
        lcb_install_callback3(instance, LCB_CALLBACK_DEFAULT, cbwrap);
    }
    return ret;
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
    map<string,map<string,string> >::iterator iter;
    string server = rs->server;
    string key((const char*)rs->key, rs->nkey);
    string value((const char *)rs->value, rs->nvalue);
    iter = stats.find(key);
    if (iter == stats.end()) {
        stats[key] = map<string,string>();
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
    vector<ServerReply>::const_iterator ii;
    for (ii = sinfo.begin(); ii != sinfo.end(); ii++) {
        if (ii->master) {
            return *ii;
        }
    }
    return dummy;
}

BatchContext::BatchContext(Client& h) : entered(false), parent(h)
{
    reset();
}

BatchContext::~BatchContext() {
    if (entered) {
        bail();
    }

    map<string,GetOperation*>::iterator ii;
    for (ii = resps.begin(); ii != resps.end(); ii++) {
        delete ii->second;
    }
}

void
BatchContext::bail() {
    entered = false;
    lcb_sched_fail(parent.getLcbt());
}

void
BatchContext::submit() {
    entered = false;
    parent.remaining += m_remaining;
    lcb_sched_leave(parent.getLcbt());
}

void
BatchContext::reset() {
    entered = true;
    m_remaining = 0;
    lcb_sched_enter(parent.getLcbt());
}

Status
BatchContext::get(const string& key) {
    GetOperation *cmd = new GetOperation(key);
    resps[key] = cmd;
    return cmd->schedule(*this);
}

const GetResponse&
BatchContext::valueFor(const string& s)
{
    static GetResponse dummy;
    GetOperation *op = resps[s];
    if (op != NULL) {
        return op->response();
    } else {
        return dummy;
    }
}
