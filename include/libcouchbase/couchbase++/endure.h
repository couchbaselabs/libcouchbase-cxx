#ifndef LCB_PLUSPLUS_H
#error "include <libcouchbase/couchbase++.h> first"
#endif

#ifndef LCB_PLUSPLUS_ENDURE_H
#define LCB_PLUSPLUS_ENDURE_H

namespace Couchbase {

EndureContext::EndureContext(Client& c,
    const DurabilityOptions& options, Handler *handler, Status& rc) : client(c) {
    rc = client.mctx_endure(options, handler, m_ctx);
}

void
EndureContext::bail() {
    m_remaining = 0;
    m_ctx.bail();
}

Status
EndureContext::add(const EndureCommand& cmd) {
    Status st = m_ctx.add(&cmd);
    if (st) {
        m_remaining++;
    }
    return st;
}

Status
EndureContext::submit() {
    Status st = m_ctx.done();
    if (st) {
        client.remaining += m_remaining;
    }
    return st;
}
}

#endif
