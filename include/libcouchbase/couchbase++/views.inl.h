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
ViewCommand::stale(StaleMode mode) {
    if (mode == StaleMode::STALE_OK) { add_option("stale", "ok"); }
    else if (mode == StaleMode::STALE_FALSE) { add_option("stale", "false"); }
    else { add_option("stale", "update_after"); }
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

char *
ViewRow::detatch_buf(Buffer& tgt, char *tmp) {
    memcpy(tmp, tgt.data(), tgt.size());
    tgt = Buffer(tmp, tgt.size());
    return tmp + tgt.size();

}

ViewRow::ViewRow(Client& c, const lcb_RESPVIEWQUERY *resp) {
    assert(! (resp->rflags & LCB_RESP_F_FINAL) );
    m_key = Buffer(resp->key, resp->nkey);
    m_value = Buffer(resp->value, resp->nvalue);
    m_docid = Buffer(resp->docid, resp->ndocid);
    m_geometry = Buffer(resp->geometry, resp->ngeometry);

    if (resp->docresp) {
        m_hasdoc = true;
        m_document.handle_response(c, LCB_CALLBACK_GET,
            reinterpret_cast<const lcb_RESPBASE*>(resp->docresp));
    }
}

void
ViewRow::detatch() {
    if (m_buf != NULL) {
        return;
    }

    size_t total_alloc = m_key.length() + m_value.length() +
            m_docid.length() + m_geometry.length();
    char *tmp = new char[total_alloc];
    m_buf.reset(tmp, std::default_delete<char[]>());

    tmp = detatch_buf(m_key, tmp);
    tmp = detatch_buf(m_value, tmp);
    tmp = detatch_buf(m_docid, tmp);
    tmp = detatch_buf(m_geometry, tmp);
}

ViewMeta::ViewMeta(const lcb_RESPVIEWQUERY *resp) : m_htcode(-1) {
    m_rc = resp->rc;
    if (resp->nvalue) {
        m_body.assign(resp->value, resp->nvalue);
    }
    if (resp->htresp) {
        m_htcode = resp->htresp->rc;
        if (resp->htresp->nbody) {
            m_body.assign((const char*)resp->htresp->body, resp->htresp->nbody);
        }
    }
}

CallbackViewQuery::CallbackViewQuery(
    Client& client, const ViewCommand& cmd, Status& status,
    RowCallback rowcb, DoneCallback donecb)
: cli(client), m_rowcb(rowcb), m_donecb(donecb) {

    status = lcb_view_query(client.handle(), this, &cmd);
    if (status) {
        vh = cmd.vhptr;
    }
}

void
CallbackViewQuery::_dispatch(const lcb_RESPVIEWQUERY *resp) {
    if (!(resp->rflags & LCB_RESP_F_FINAL)) {
        m_rowcb(ViewRow(cli, resp), this);
    } else {
        m_donecb(ViewMeta(resp), this);
        vh = NULL;
    }
}

void
CallbackViewQuery::stop() {
    if (active()) {
        lcb_view_cancel(cli.handle(), vh);
        vh = NULL;
    }
}

ViewQuery::ViewQuery(Client& cli, const ViewCommand& cmd, Status& st)
: CallbackViewQuery(cli, cmd, st,

    [this](ViewRow&& r, CallbackViewQuery*) {
        handle_row(std::move(r));
    },
    [this](ViewMeta&& m, CallbackViewQuery*) {
        handle_done(std::move(m));
    })
{
}

CallbackViewQuery::~CallbackViewQuery() {
    stop();
}

void
ViewQuery::handle_row(ViewRow&& row) {
    row.detatch();
    rp_add(std::move(row));
    cli.breakout();
}

void
ViewQuery::handle_done(ViewMeta&& m) {
    m_meta = std::move(m);
    cli.breakout();
}

Status
ViewQuery::status() const {
    assert(!active());
    return m_meta.status();
}
} // namespace Couchbase
