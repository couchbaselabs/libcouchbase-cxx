#include <libcouchbase/lcb++.h>
#include <stdio.h>
using namespace Couchbase;
using std::string;
using std::map;
using std::vector;

extern "C" {
static void cbwrap(lcb_t instance, int cbtype,
    const lcb_RESPBASE *res)
{
    Client::dispatchFromCallback(instance, cbtype, res);
}
}

void
Client::dispatchFromCallback(lcb_t instance, int cbtype,
    const lcb_RESPBASE *resp)
{
    Client *h = reinterpret_cast<Client*>((void*)lcb_get_cookie(instance));
    h->dispatch(cbtype, resp);
}

void
Client::dispatch(int cbtype, const lcb_RESPBASE *r)
{
    Response *resp = (Response *)r->cookie;
    resp->init(r);
}

Client::Client(const char *connstr, const char *passwd)
{
    lcb_create_st cropts;
    cropts.version = 3;
    cropts.v.v3.connstr = connstr;
    cropts.v.v3.passwd = passwd;
    Status rv = lcb_create(&instance, &cropts);
    if (rv != LCB_SUCCESS) {
        throw rv;
    }
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


Response
Client::store(const string& key, const string& value)
{
    StoreOperation op(LCB_SET);
    op.key(key);
    op.value(value);
    op.run(*this);
    return op.response();
}

void
GetResponse::init(const lcb_RESPBASE *resp)
{
    u.get = *(lcb_RESPGET *)resp;
    if (status()) {
        lcb_backbuf_ref((lcb_BACKBUF )u.get.bufh);
    } else {
        u.get.bufh = NULL;
    }
}

void
GetResponse::clear()
{
    if (u.get.bufh != NULL) {
        lcb_backbuf_unref((lcb_BACKBUF)u.get.bufh);
        u.get.bufh = NULL;
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
    lcb_sched_leave(parent.getLcbt());
}
void
BatchContext::reset() {
    entered = true;
    lcb_sched_enter(parent.getLcbt());
}

Status
BatchContext::get(const string& key) {
    GetOperation *cmd = new GetOperation(key);
    resps[key] = cmd;
    return cmd->schedule(*this);
}

const GetResponse&
BatchContext::operator [](const string& s)
{
    static GetResponse dummy;
    GetOperation *op = resps[s];
    if (op != NULL) {
        return op->response();
    } else {
        return dummy;
    }
}
