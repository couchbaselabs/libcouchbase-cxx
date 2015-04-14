namespace Couchbase {

ViewCommand::ViewCommand(const char *design, const char *view) {
    s_design = design;
    s_view = view;
    vhptr = NULL;
    memset((lcb_CMDVIEWQUERY*)this, 0, sizeof(lcb_CMDVIEWQUERY));

    this->view = s_view.c_str();
    this->nview = s_view.length();
    this->ddoc = s_design.c_str();
    this->nddoc = s_design.length();
    this->handle = &vhptr;
    this->callback = Internal::viewcb;
}

void
ViewCommand::add_option(const char *key, const char *value) {
    m_options += key;
    m_options += "=";
    m_options += value;
    m_options += '&';

    optstr = m_options.c_str();
    noptstr = m_options.size();
}

void
ViewCommand::add_option(const char *key, bool value) {
    add_option(key, value ? "true" : "false");
}

void
ViewCommand::add_option(const char *key, int val) {
    char buf[32];
    sprintf(buf, "%d", val);
    add_option(key, buf);
}

void
ViewCommand::options(const char *options) {
    m_options = options;
    optstr = m_options.c_str();
    noptstr = m_options.size();
}


void
ViewCommand::add_cmd_flag(int flag, bool enabled) {
    if (enabled) {
        cmdflags |= flag;
    } else {
        cmdflags &= ~flag;
    }
}

void
ViewRow::f2s(const void *b, size_t n, std::string& s) {
    if (n) { s.assign((const char*)b, n); }
}

ViewRow::ViewRow(const lcb_RESPVIEWQUERY *resp) {
    assert(! (resp->rflags & LCB_RESP_F_FINAL) );
    f2s(resp->key, resp->nkey, m_key);
    f2s(resp->value, resp->nvalue, m_value);
    f2s(resp->geometry, resp->ngeometry, m_geometry);
    f2s(resp->docid, resp->ndocid, m_docid);

    if (resp->docresp) {
        m_hasdoc = true;
        m_document.init((const lcb_RESPBASE*)resp->docresp);
    }
}

ViewMeta::ViewMeta(const lcb_RESPVIEWQUERY *resp) : htcode(-1) {
    rc = resp->rc;
    if (resp->nvalue) {
        body.assign(resp->value, resp->nvalue);
    }
    if (resp->htresp) {
        htcode = resp->htresp->rc;
        if (resp->htresp->nbody) {
            body.assign((const char*)resp->htresp->body, resp->htresp->nbody);
        }
    }
}

ViewQuery::ViewQuery(Client& client, const ViewCommand& cmd, Status& status) :
    cli(client), m_meta(NULL) {
    status = lcb_view_query(client.handle(), this, &cmd);
    if (status) {
        vh = cmd.vhptr;
    }
}

ViewQuery::~ViewQuery() {
    stop();
    if (m_meta != NULL) {
        delete m_meta;
        m_meta = NULL;
    }
}

void
ViewQuery::_dispatch(const lcb_RESPVIEWQUERY *resp) {
    if (!(resp->rflags & LCB_RESP_F_FINAL)) {
        rows.push_back(ViewRow(resp));
    } else {
        m_meta = new ViewMeta(resp);
        vh = NULL;
    }
    cli.breakout();
}

void
ViewQuery::stop() {
    if (active()) {
        lcb_view_cancel(cli.handle(), vh);
        vh = NULL;
    }
}

const ViewRow*
ViewQuery::next() {
    GT_RETRY:
    if (!rows.empty()) {
        return &rows.front();
    }
    if (!active()) {
        return NULL;
    }
    cli.wait();
    goto GT_RETRY;
}

Status
ViewQuery::status() const {
    assert(!active());
    if (m_meta != NULL) {
        return m_meta->rc;
    }
    return Status(LCB_ERROR);
}

namespace Internal {
class ViewIterator {
public:
    const ViewRow& operator*() const {
        return *pp;
    }
    const ViewRow* operator->() const {
        return pp;
    }
    ViewIterator(const ViewIterator& other) : q(other.q) {
        pp = other.pp;
    }
    ViewIterator& operator=(const ViewIterator& other) {
        q = other.q;
        pp = other.pp;
        return *this;
    }
    ViewIterator& operator++() {
        q->rows.pop_front();
        pp = q->next(); return *this;
    }
    ViewIterator& operator++(int) {
        return operator ++();
    }
    bool operator!=(const ViewIterator& other) const {
        return pp != other.pp;
    }
    bool operator==(const ViewIterator& other) const {
        return pp == other.pp && q == other.q;
    }
private:
    friend class Couchbase::ViewQuery;
    ViewQuery* q;
    const ViewRow *pp;
    ViewIterator(ViewQuery* query) :q(query), pp(NULL) {}
    ViewIterator& mkend() { pp = NULL; return *this; }
    ViewIterator& mkbegin() { pp = q->next(); return *this; }
};
} // namespace Internal

ViewQuery::const_iterator ViewQuery::begin() {
    return const_iterator(this).mkbegin();
}

ViewQuery::const_iterator ViewQuery::end() {
    return const_iterator(this).mkend();
}
} // namespace Couchbase
