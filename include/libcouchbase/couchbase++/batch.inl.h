#ifndef LCB_PLUSPLUS_H
#error "include <libcouchbase/couchbase++.h> first!"
#endif


namespace Couchbase {

Context::Context(Client& h) : parent(h)
{
    reset();
}

Context::~Context() {
    if (entered) {
        bail();
    }
}

Context::Context(Context&& other)
: parent(other.parent) {
    *this = std::move(other);
}

Context&
Context::operator =(Context&& other) {
    entered = other.entered;
    m_remaining = other.m_remaining;

    other.entered = false;
    other.m_remaining = 0;
    return *this;
}

template <typename T> Status
Context::add(const Command<T>& cmd, Handler *handler) {
    Status st = cmd.scheduler()(parent.handle(), handler->as_cookie(), &cmd);
    if (st) {
        m_remaining++;
    }
    return st;
}

void
Context::bail() {
    entered = false;
    parent.fail();
}

void
Context::submit() {
    entered = false;
    parent.remaining += m_remaining;
    parent.leave();
}

void
Context::reset() {
    entered = true;
    m_remaining = 0;
    parent.enter();
}

// Batched commands
template <typename C, typename R>
BatchCommand<C,R>::BatchCommand(Client &c) : m_ctx(c) {
}

template <typename C, typename R> Status
BatchCommand<C,R>::add(const C& cmd) {
    m_resplist.push_back(R());
    return m_ctx.add(cmd, &m_resplist.back());
}

// Callback stuff
template <typename C, typename R>
CallbackCommand<C,R>::CallbackCommand(Client& c, CallbackType& cb)
: m_callback(cb), m_ctx(c) {
}

template <typename C, typename R> Status
CallbackCommand<C,R>::add(const C& cmd) {
    return m_ctx.add(cmd, this);
}

template <typename C, typename R> void
CallbackCommand<C,R>::handle_response(Client& c, int t, const lcb_RESPBASE *r) {
    R resp;
    resp.handle_response(c, t, r);
    resp.set_key(r);
    m_callback(resp);
}

} //namespace Couchbase
