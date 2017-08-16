namespace Couchbase {

QueryCommand::QueryCommand(const std::string& stmt) {
    m_params = lcb_n1p_new();
    lcb_n1p_setquery(m_params, stmt.c_str(), stmt.size(),
        LCB_N1P_QUERY_STATEMENT);
}

QueryCommand::QueryCommand(QueryCommand&& other) {
    m_adhoc = other.m_adhoc;
    m_params = other.m_params;
    other.m_params = NULL;
}

QueryCommand::~QueryCommand() {
    if (m_params != NULL) {
        lcb_n1p_free(m_params);
    }
}

Status
QueryCommand::raw_option(const std::string& name, const std::string& value) {
    return lcb_n1p_setopt(m_params,
        name.c_str(), name.size(), value.c_str(), value.size());
}
Status
QueryCommand::named_param(const std::string& name, const std::string& value) {
    return lcb_n1p_namedparam(m_params,
        name.c_str(), name.size(), value.c_str(), value.size());
}

Status
QueryCommand::pos_param(const std::string& value) {
    return lcb_n1p_posparam(m_params, value.c_str(), value.size());
}

void
QueryMeta::assign(QueryMeta&& other)
{
    m_buf = std::move(other.m_buf);
    m_httpbuf = std::move(other.m_httpbuf);
    m_status = other.m_status;
    m_httpcode = other.m_httpcode;
}

QueryMeta::QueryMeta(const lcb_RESPN1QL *resp)
: m_status(resp->rc) {
    if (resp->nrow) {
        m_buf.assign(resp->row, resp->nrow);
    }
    if (resp->htresp) {
        auto htr = resp->htresp;
        m_httpcode = htr->htstatus;
        if (htr->nbody) {
            m_httpbuf.assign(static_cast<const char*>(htr->body), htr->nbody);
        }
    }
}

QueryRow::QueryRow(const lcb_RESPN1QL *resp) {
    m_row = Buffer(resp->row, resp->nrow);
}

void
QueryRow::detatch() {
    if (m_buf == NULL && !m_row.empty()) {
        char *tmp = new char[m_row.size()];
        m_buf.reset(tmp, std::default_delete<char[]>());
        memcpy(tmp, m_row.data(), m_row.size());
        m_row = Buffer(tmp, m_row.size());
    }
}

namespace Internal {
extern "C" {
static void n1qlcb(lcb_t, int, const lcb_RESPN1QL *resp) {
    auto query = reinterpret_cast<CallbackQuery*>(resp->cookie);
    query->_dispatch(resp);
}
}
} // namespace Internal


CallbackQuery::CallbackQuery(Client& client, QueryCommand& cmd, Status &status,
    RowCallback rowcb, DoneCallback donecb)
: m_cli(client), m_rowcb(rowcb), m_donecb(donecb) {

    lcb_CMDN1QL c_cmd = { 0 };
    c_cmd.callback = Internal::n1qlcb;
    if (!cmd.m_adhoc) {
        c_cmd.cmdflags |= LCB_CMDN1QL_F_PREPCACHE;
    }

    status = lcb_n1p_mkcmd(cmd.m_params, &c_cmd);
    if (status) {
        status = lcb_n1ql_query(m_cli.handle(), this, &c_cmd);
    }
}

void
CallbackQuery::_dispatch(const lcb_RESPN1QL *resp) {
    if (resp->rflags & LCB_RESP_F_FINAL) {
        // Handle last response..
        m_done = true;
        m_donecb(QueryMeta(resp), this);
    } else {
        m_rowcb(QueryRow(resp), this);
    }
}

Query::Query(Client& cli, QueryCommand& cmd, Status& st)
: CallbackQuery(cli, cmd, st,
    [this](QueryRow&& row, CallbackQuery*){
        row.detatch();
        rp_add(std::move(row));
        this->m_cli.breakout();
    },
    [this](QueryMeta&& meta, CallbackQuery*){
        m_meta = std::move(meta);
        this->m_cli.breakout();
    })
{
}

QueryMeta
Query::execute(Client& client, const std::string& s) {
    QueryCommand cmd(s);
    Status st;
    Query q(client, cmd, st);

    if (!st) {
        QueryMeta qm;
        qm.m_status = st;
        return qm;
    }

    for (auto i : q) {
        // Do nothing
    }
    return std::move(q.m_meta);
}

}
