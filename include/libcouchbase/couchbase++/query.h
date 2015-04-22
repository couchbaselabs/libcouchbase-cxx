#include <libcouchbase/n1ql.h>
namespace Couchbase {
namespace Internal {
extern "C" { static void n1qlcb(lcb_t,int,const lcb_RESPN1QL*); }
}

class QueryCommand {
public:
    inline QueryCommand(const std::string& statement);
    inline ~QueryCommand();
    inline Status raw_option(const std::string& name, const std::string &value);
    inline Status named_param(const std::string& name, const std::string& value);
    inline Status pos_param(const std::string& value);
    inline void clear();
private:
    friend class CallbackQuery;
    QueryCommand(QueryCommand&) = delete;
    QueryCommand& operator=(QueryCommand&) = delete;
    lcb_N1QLPARAMS *m_params;
};

class QueryRow {
public:
    inline void detatch();
    const Buffer& json() const { return m_row; }
    operator std::string() const { return m_row.to_string(); }
private:
    friend class CallbackQuery;
    QueryRow(const lcb_RESPN1QL *);
    Buffer m_row;
    std::shared_ptr<char> m_buf;
};

class QueryMeta {
public:
    QueryMeta& operator=(QueryMeta&& other) { assign(std::move(other)); return *this; }
    QueryMeta(QueryMeta&& other) { assign(std::move(other)); }
    QueryMeta() {}
    Status status() const { return m_status; }
    Buffer body() const { return Buffer(m_buf.c_str(), m_buf.size()); }
    Buffer http_body() const { return Buffer(m_httpbuf.c_str(), m_httpbuf.size()); }
    short http_status() const { return m_httpcode; }
private:
    friend class CallbackQuery;
    friend class Query;
    inline void assign(QueryMeta&&);

    inline QueryMeta(const lcb_RESPN1QL *resp);
    std::string m_buf;
    std::string m_httpbuf;
    Status m_status;
    short m_httpcode = 0;
};

class CallbackQuery {
public:
    typedef std::function<void(QueryRow&&, CallbackQuery*)> RowCallback;
    typedef std::function<void(QueryMeta&&, CallbackQuery*)> DoneCallback;

    inline CallbackQuery(Client&, QueryCommand&, Status& status,
        RowCallback rowcb, DoneCallback donecb);

    virtual inline ~CallbackQuery() {}
    inline void _dispatch(const lcb_RESPN1QL*);
    bool active() const { return !m_done; }
protected:
    Client& cli;
private:
    RowCallback rowcb = NULL;
    DoneCallback donecb = NULL;
    bool m_done = false;

};

class Query : public CallbackQuery, protected Internal::RowProvider<QueryRow> {
public:
    inline Query(Client&, QueryCommand&, Status&);
    typedef Internal::RowIterator<QueryRow> const_iterator;
    const_iterator begin() { return rp_begin(); }
    const_iterator end() { return rp_end(); }
    const QueryMeta& meta() const { return m_meta; }
    Status status() const { return m_meta.status(); }

    inline static QueryMeta execute(Client& client, const std::string& stmt);

protected:
    bool rp_active() const override { return active(); }
    void rp_wait() override { cli.wait(); }
private:
    QueryMeta m_meta;
};

}

#include <libcouchbase/couchbase++/query.inl.h>
