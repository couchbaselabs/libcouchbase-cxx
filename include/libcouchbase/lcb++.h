#ifndef LCB_PLUSPLUS_H
#define LCB_PLUSPLUS_H

#include <libcouchbase/couchbase.h>
#include <libcouchbase/api3.h>
#include <libcouchbase/pktfwd.h>
#include <string>
#include <stdint.h>
#include <map>
#include <vector>
#include "lcb++-internal.h"

namespace Couchbase {

class Status {
public:
    Status(lcb_error_t rc=LCB_SUCCESS) : code(rc) { }
    bool success() const { return code == LCB_SUCCESS; }
    bool isNetworkError() const { return LCB_EIFNET(code) != 0; }
    bool isInputError() const { return LCB_EIFINPUT(code) != 0; }
    bool isDataError() const { return LCB_EIFDATA(code) != 0; }
    bool isTemporary() const { return LCB_EIFTMP(code) != 0; }
    const char *description() const { return lcb_strerror(NULL, code); }
    lcb_error_t errcode() const { return code; }
    operator bool() const { return success(); }
    operator lcb_error_t() const { return code; }
    operator const char * () const { return  description(); }
    Status(const Status& other) : code(other.code) {}
private:
    lcb_error_t code;
};


class Command {
public:
    void key(const char *buf, size_t len) { LCB_CMD_SET_KEY(&m_base, buf, len); }
    void key(const char *buf) { key(buf, strlen(buf)); }
    void key(const std::string& s) { key(s.c_str(), s.size()); }
    void expiry(unsigned n) { m_base.exptime = n; }
    void cas(uint64_t casval) { m_base.cas = casval; }
    const lcb_CMDBASE* asCmdBase() const { return &m_base; }
    Command() { memset(this, 0, sizeof *this); }

    LCB_CXX_CMD_IS(lcb_CMDBASE, m_base)

protected:
    union { LCB_CXX_UREQUESTS };
};

class GetCommand : public Command {
public:
    LCB_CXX_CMD_CTOR(GetCommand)
    LCB_CXX_CMD_IS(lcb_CMDGET, m_get)
    void locktime(unsigned n) { expiry(n); m_get.lock = 1; }
};

class StoreCommand : public Command {
public:
    StoreCommand(lcb_storage_t op) : Command() {m_store.operation=op;}
    void mode(lcb_storage_t op) { m_store.operation = op; }
    void value(const void *b, size_t n) { LCB_CMD_SET_VALUE(&m_store, b, n); }
    void value(const char *s) { value(s, strlen(s)); }
    void value(const std::string& s) { value(s.c_str(), s.size()); }
    void itemflags(uint32_t f) { m_store.flags = f; }
    LCB_CXX_CMD_IS(lcb_CMDSTORE, m_store);
};

class CounterCommand : public Command {
public:
    CounterCommand() : Command() { }
    CounterCommand(int64_t delta) : Command() { m_arith.delta = delta; }
    void deflval(uint64_t d) { m_arith.initial = d; m_arith.create = 1; }
    LCB_CXX_CMD_IS(lcb_CMDCOUNTER, m_arith);
};

class ObserveCommand : public Command {
public:
    LCB_CXX_CMD_CTOR(ObserveCommand)
    LCB_CXX_CMD_IS(lcb_CMDOBSERVE, m_obs);
    void master_only(bool val=true) {
        if (val) { m_obs.cmdflags |= LCB_CMDOBSERVE_F_MASTER_ONLY; }
        else { m_obs.cmdflags &= ~LCB_CMDOBSERVE_F_MASTER_ONLY; }
    }
};

class RemoveCommand : public Command {
public:
    LCB_CXX_CMD_CTOR(RemoveCommand)
    LCB_CXX_CMD_IS(lcb_CMDREMOVE, m_base)
};

class EndureOptions : public lcb_DURABILITYOPTSv0 {
public:
    EndureOptions() {memset(this, 0, sizeof *this);persist_to = 1;cap_max = 1;}
};

typedef Command TouchCommand;
typedef Command EndureCommand;

class Client;
struct ResponseBase {
    Internal::uResponses u;
    Status status() const { return u.base.rc; }
    bool final() const { return u.base.rflags & LCB_RESP_F_FINAL; }
    void *cookie() const { return (void *)u.base.cookie; }
    uint64_t cas() const { return u.base.cas; }
    void fail(Status& rv) { u.base.rc = rv; }
};

class Response : public ResponseBase {
    friend class Client;

public:
    Response() {}
    virtual void init(const lcb_RESPBASE *res) { u.base = *res; }
    virtual ~Response() {}
private:
};

class GetResponse : public Response {
public:
    GetResponse() : Response() { u.get.bufh = NULL; }
    ~GetResponse() { clear(); }
    void clear();
    const char* valuebuf() const { return (const char *)u.get.value; }
    size_t valuesize() const { return u.get.nvalue; }
    void value(std::string& s) const {
        if (!status()) { return; }
        s.assign(valuebuf(), valuesize());
    }

    std::string value() const { std::string s; value(s); return s; }
    uint32_t valueflags() const { return u.get.itmflags; }
    void init(const lcb_RESPBASE *);

protected:
    friend class Client;
    GetResponse(GetResponse& other);
};

class StatsResponse : public Response {
public:
    StatsResponse() : Response(), initialized(false) { }
    void init(const lcb_RESPBASE *resp);
    std::map<std::string,std::map<std::string,std::string> > stats;
private:
    bool initialized;
};

class CounterResponse : public Response {
public:
    void init(const lcb_RESPBASE *res) { u.arith = *(lcb_RESPCOUNTER *)res; }
};

class EndureResponse : public Response {
public:
    void init(const lcb_RESPBASE *res) { u.endure = *(lcb_RESPENDURE*)res; }
    bool on_master_storage() const { return u.endure.persisted_master ? true : false; }
    bool on_master_ram() const { return u.endure.exists_master ? true : false; }
    size_t probes() const { return u.endure.nresponses; }
    size_t persisted() const { return u.endure.npersisted; }
    size_t replicate() const { return u.endure.nreplicated; }
};


class ObserveResponse : public Response {
public:
    struct ServerReply {
        uint64_t cas;
        bool master;
        uint8_t status;
        ServerReply() : cas(0), master(false), status(0) {}
        bool exists() const { return status == LCB_OBSERVE_FOUND; }
        bool persisted() const { return status & (int)LCB_OBSERVE_PERSISTED; }
    };

    ObserveResponse() : Response(), initialized(false) { }
    void init(const lcb_RESPBASE *res);
    const ServerReply& master_reply() const;
    const std::vector<ServerReply>& all_replies() const { return sinfo; }
private:
    bool initialized;
    std::vector<ServerReply> sinfo;
};


class BatchContext {
public:
    BatchContext(Client&);
    ~BatchContext();
    Status get(const std::string& s);
    void bail();
    void submit();
    void reset();
    const GetResponse& operator[](const std::string&s);
    inline lcb_t getLcbt() const;
private:
    friend class Command;
    bool entered;
    Client& parent;
    Response dummy;
    std::map<std::string,Operation<GetCommand,GetResponse>*> resps;
};

class Client {
public:
    Client(const char *s, const char *passwd = NULL);
    ~Client();
    Status connect();
    GetResponse get(const std::string& key);
    Response store(const std::string& key, const std::string& value);
    Response remove(const std::string& key);
    Response touch(const std::string& key, unsigned expiry=0);
    CounterResponse incr(const std::string& key);
    CounterResponse decr(const std::string& key);
    void wait();
    lcb_t getLcbt() const { return instance; }

    static void dispatchFromCallback(lcb_t, int, const lcb_RESPBASE*);
private:
    Status schedule(const Command&, Response*, lcb_CALLBACKTYPE op);
    lcb_t instance;
    void dispatch(int, const lcb_RESPBASE*);
    Client(Client&);
};

class Client;
template <typename C=Command, typename R=Response>
class Operation : public C {
public:
    typedef C _Cmdcls;
    typedef R _Respcls;
    LCB_CXX_OP_CTOR(Operation)
    inline Operation(lcb_storage_t);

    R& response() { return res; }
    Status schedule(BatchContext& ctx) { return scheduleLcb(ctx.getLcbt()); }
    Status run(Client& h) {
        lcb_t instance = h.getLcbt();
        Status st = scheduleLcb(instance);
        if (!st) {
            lcb_sched_fail(instance);
            return st;
        } else {
            lcb_sched_leave(instance);
            lcb_wait3(instance, LCB_WAIT_NOCHECK);
            return res.status();
        }
    }
protected:
    inline Status scheduleLcb(lcb_t);
    R res;
};

typedef Operation<RemoveCommand,Response> RemoveOperation;
typedef Operation<TouchCommand,Response> TouchOperation;
typedef Operation<CounterCommand,Response> CounterOperation;
typedef Operation<EndureCommand,EndureResponse> EndureOperation;
typedef Operation<ObserveCommand,ObserveResponse> ObserveOperation;
typedef Operation<GetCommand,GetResponse> GetOperation;
typedef Operation<StoreCommand,Response> StoreOperation;
lcb_t BatchContext::getLcbt() const { return parent.getLcbt(); }
}
#include "lcb++-inldefs.h"


#endif
