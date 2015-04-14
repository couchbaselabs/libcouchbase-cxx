#ifndef LCB_PLUSPLUS_H
#error "include <libcouchbase/couchbase++.h> first!"
#endif


namespace Couchbase {

BatchContext::BatchContext(Client& h) : entered(false), parent(h)
{
    reset();
}

BatchContext::~BatchContext() {
    if (entered) {
        bail();
    }

    for (auto ii = resps.begin(); ii != resps.end(); ii++) {
        delete ii->second;
    }
}

void
BatchContext::bail() {
    entered = false;
    lcb_sched_fail(parent.handle());
}

void
BatchContext::submit() {
    entered = false;
    parent.remaining += m_remaining;
    lcb_sched_leave(parent.handle());
}

void
BatchContext::reset() {
    entered = true;
    m_remaining = 0;
    lcb_sched_enter(parent.handle());
}

Status
BatchContext::get(const std::string& key) {
    GetOperation *cmd = new GetOperation(key);
    resps[key] = cmd;
    return cmd->schedule(*this);
}

const GetResponse&
BatchContext::value_for(const std::string& s)
{
    static GetResponse dummy;
    GetOperation *op = resps[s];
    if (op != NULL) {
        return op->const_response();
    } else {
        return dummy;
    }
}
} //namespace Couchbase
