#include <libcouchbase/n1ql.h>
namespace Couchbase {
namespace Internal {
extern "C" { static void n1qlcb(lcb_t,int,const lcb_RESPN1QL*); }
}

class QueryCommand {
public:
    /**
     * Create the query command
     * @param statement The N1QL statement to use
     */
    inline QueryCommand(const std::string& statement);
    inline QueryCommand(QueryCommand&& other);
    inline ~QueryCommand();
    /**
     * Set a raw query option
     * @param name The name of the option
     * @param value The value of the option. Must be parseable as JSON
     * @return error code. Should succeed if value is properly formatter JSON
     */
    inline Status raw_option(const std::string& name, const std::string &value);

    /**
     * Set a named parameter for the query
     * @param name The named query parameter (excluding the preceding `$`)
     * @param value The value for the parameter. Should be JSON parseable
     * @return success, or error if value is not properly formatted JSON
     */
    inline Status named_param(const std::string& name, const std::string& value);

    /**
     * Add positional parameters for the query string. The positional parameters
     * should follow the placeholders in the query string
     * @param value Positional value to add
     * @return Error if value is not JSON
     */
    inline Status pos_param(const std::string& value);

    /**
     * If this query object is an adhoc query. If this is set to false then
     * the query will be internally prepared by the library for faster
     * execution later on, at the cost of an initial overhead for preparation
     * @param value whether this is an adhoc query. The default is true.
     */
    void adhoc(bool value) { m_adhoc = value; }

private:
    friend class CallbackQuery;
    QueryCommand(QueryCommand&) = delete;
    QueryCommand& operator=(QueryCommand&) = delete;
    lcb_N1QLPARAMS *m_params = NULL;
    bool m_adhoc = true;
};

/**
 * A single row in a query result
 */
class QueryRow {
public:
    /**
     * Get the row contents
     * @return The JSON text from the row. This can be parsed by another
     * JSON parser.
     */
    const Buffer& json() const { return m_row; }
    /**
     * Makes the buffer scoped to the row itself. Useful if you wish to persist
     * the row data outside the callback (if using callback rows)
     */
    inline void detatch();

    operator std::string() const { return m_row.to_string(); }
private:
    friend class CallbackQuery;
    friend class Query;
    QueryRow(const lcb_RESPN1QL *);
    Buffer m_row;
    std::shared_ptr<char> m_buf;
};

/**
 * Metadata about the query itself
 */
class QueryMeta {
public:
    QueryMeta& operator=(QueryMeta&& other) { assign(std::move(other)); return *this; }
    QueryMeta(QueryMeta&& other) { assign(std::move(other)); }
    QueryMeta() {}

    /**
     * Whether the query was successful. If not successful, the body() and
     * http_body() methods may shed more light
     */
    Status status() const { return m_status; }

    /**
     * Gets the JSON body for the response. This is the result contents with
     * an empty resultset
     */
    Buffer body() const { return Buffer(m_buf.c_str(), m_buf.size()); }

    /**
     * Gets the raw HTTP body. This can be used if body() is empty. The contents
     * of this value may not always be JSON
     */
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

/**
 * Query handler which invokes a callback for each row received
 */
class CallbackQuery {
public:

    /**
     * Callback type inovked for each row
     * @param row the row received
     * @param ctx the CallbackQuery object
     */
    typedef std::function<void(QueryRow&& row, CallbackQuery *ctx)> RowCallback;

    /**
     * Callback invoked once when all rows have been received
     * @param meta metadata about the query
     * @param ctx the CallbackQuery object
     */
    typedef std::function<void(QueryMeta&& meta, CallbackQuery *ctx)> DoneCallback;

    /**
     * @param cli the client handle
     * @param cmd the query command
     * @param[out] if an error happens during query construction, it is indicated here
     * @param rowcb the callback to be invoked for each row
     * @param donecb the callback to be invoked once all rows are done
     */
    inline CallbackQuery(Client& cli, QueryCommand& cmd, Status& status,
        RowCallback rowcb, DoneCallback donecb);

    /**
     * Check if the query is still ongoing
     * @return true if there is more data to fetch, false otherwise
     */
    bool active() const { return !m_done; }

    virtual ~CallbackQuery() {}

    /**
     * @private
     * Called from the handler when a row has been received. This dispatches
     * the actual callback depending on what the response says
     */
    inline void _dispatch(const lcb_RESPN1QL*);
protected:
    Client& m_cli;
private:
    CallbackQuery(const CallbackQuery&) = delete;
    CallbackQuery& operator=(const CallbackQuery& other) = delete;
    RowCallback m_rowcb = NULL;
    DoneCallback m_donecb = NULL;
    bool m_done = false;

};

/**
 * Simple synchronous query class.
 *
 * This class can be iterated over, for example
 *
 * @code{c++}
 * Query q(client,
 */
class Query : public CallbackQuery, protected Internal::RowProvider<QueryRow> {
public:
    /**
     * @param client Client handle
     * @param cmd Command containing the statement
     * @param[out] status will contain an error if the query could not be issued
     */
    inline Query(Client& client, QueryCommand& cmd, Status& status);

    typedef Internal::RowIterator<QueryRow> const_iterator;
    const_iterator begin() { return rp_begin(); }
    const_iterator end() { return rp_end(); }
    const QueryMeta& meta() const { return m_meta; }
    Status status() const { return m_meta.status(); }

    inline static QueryMeta execute(Client& client, const std::string& stmt);

protected:
    bool rp_active() const override { return active(); }
    void rp_wait() override { m_cli.wait(); }
private:
    QueryMeta m_meta;
};

}

#include <libcouchbase/couchbase++/query.inl.h>
