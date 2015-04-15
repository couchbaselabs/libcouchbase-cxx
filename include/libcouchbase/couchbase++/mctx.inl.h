namespace Couchbase {
namespace Internal {

template <typename T>
class MultiContextT {
public:
    MultiContextT()
    : m_inner(NULL), cookie(NULL), client(NULL) {}

    MultiContextT(lcb_MULTICMD_CTX *c, Handler *handler, Client *cli)
    : m_inner(c), cookie(handler), client(cli) {
    }

    MultiContextT& operator=(MultiContextT&& other) {
        assert(m_inner == NULL);
        m_inner = other.m_inner;
        cookie = other.cookie;
        client = other.client;
        other.m_inner = NULL;
        return *this;
    }

    ~MultiContextT() { bail(); }

    Status add(const T* cmd) {
        return m_inner->addcmd(m_inner, reinterpret_cast<const lcb_CMDBASE*>(cmd));
    }

    Status done() {
        lcb_sched_enter(client->handle());
        Status s = m_inner->done(m_inner, cookie->as_cookie());
        if (!s) {
            lcb_sched_fail(client->handle());
        } else {
            lcb_sched_leave(client->handle());
        }
        m_inner = NULL;
        return s;
    }

    void bail() {
        if (m_inner != NULL) {
            m_inner->fail(m_inner);
            m_inner = NULL;
        }
    }

private:
    lcb_MULTICMD_CTX *m_inner;
    Handler *cookie;
    Client *client;
    MultiContextT(MultiContextT&) = delete;
    MultiContextT& operator=(MultiContextT&) = delete;
};

}
}
