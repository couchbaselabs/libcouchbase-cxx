namespace Couchbase {
namespace Internal {

template<typename T>
MultiContextT<T>::MultiContextT()
: m_inner(NULL), cookie(NULL), client(NULL) {
}

template<typename T>
MultiContextT<T>::MultiContextT(lcb_MULTICMD_CTX *c, Handler *handler, Client *cli)
: m_inner(c), cookie(handler), client(cli) {
}

template <typename T> MultiContextT<T>&
MultiContextT<T>::operator=(MultiContextT<T>&& other) {
    assert(m_inner == NULL);
    m_inner = other.m_inner;
    cookie = other.cookie;
    client = other.client;
    other.m_inner = NULL;
    return *this;
}

template<typename T>
MultiContextT<T>::~MultiContextT() {
    bail();
}

template <typename T> Status
MultiContextT<T>::add(const T* cmd) {
    return m_inner->addcmd(m_inner, reinterpret_cast<const lcb_CMDBASE*>(cmd));
}

template <typename T> Status
MultiContextT<T>::done() {
    client->enter();
    Status s = m_inner->done(m_inner, cookie->as_cookie());
    if (!s) {
        client->fail();
    } else {
        client->leave();
    }
    m_inner = NULL;
    return s;
}

template <typename T> void
MultiContextT<T>::bail() {
    if (m_inner != NULL) {
        m_inner->fail(m_inner);
        m_inner = NULL;
    }
}

} // namespace Couchbase
} // namespace Internal
