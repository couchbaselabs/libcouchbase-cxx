#ifndef LCB_PLUSPLUS_H
#error "Include <libcouchbase/couchbase++.h> first!"
#endif

#ifndef LCB_PLUSPLUS_VIEWS_H
#define LCB_PLUSPLUS_VIEWS_H

#include <deque>
#include <libcouchbase/couchbase++/row_common.h>

namespace Couchbase {

class CallbackViewQuery;
class ViewQuery;

namespace Internal {
extern "C" { static void viewcb(lcb_t,int,const lcb_RESPVIEWQUERY*); }
}

enum class StaleMode { STALE_OK, STALE_FALSE, STALE_UPDATE_AFTER };

// View API
class ViewCommand : private lcb_CMDVIEWQUERY {
public:
    //! @Create a new view command
    //! @param design the design document
    //! @param view the view to execute
    inline ViewCommand(const char *design, const char *view);

    //! Indicate if this is a spatial view
    //! @param enabled true if this is a spatial view, false otherwise
    inline void spatial(bool enabled = true) {
        add_cmd_flag(LCB_CMDVIEWQUERY_F_SPATIAL, enabled);
    }

    //! Whether to include documents with each row.
    //! If enabled, each @ref ViewRow object will contain a valid `document`
    //! field.
    //! @param enabled true if documents should be fetched alongside items
    void include_docs(bool enabled = true) {
        add_cmd_flag(LCB_CMDVIEWQUERY_F_INCLUDE_DOCS, enabled);
    }

    //! Do not parse rows into id, key, value and geometry fields.
    //! Use this if using an actual JSON parser which may be more efficient
    //! if given an entire row rather than distinct chunks. Note this option
    //! is incompatible with #include_docs().
    //! @param enabled whether to enable this feature
    void no_parse_rows(bool enabled = true) {
        add_cmd_flag(LCB_CMDVIEWQUERY_F_NOROWPARSE, enabled);
    }

    //! Add a simple view option. The option must be properly formatted
    //! @param key the option name
    //! @param value the option value
    inline void add_option(const char *key, const char *value);
    inline void add_option(const char *key, bool value);
    inline void add_option(const char *key, int value);

    // More specific view options
    void skip(int value) { add_option("skip", value); }
    void limit(int value) { add_option("limit", value); }
    void descending(bool value) { add_option("descending", value); }
    void reduce(bool value) { add_option("descending", value); }
    void group(bool value) { add_option("group", value); }
    void group_level(int value) { add_option("group_level", value); }
    inline void stale(StaleMode mode);

    //! Set the raw option string, e.g. `"stale=false&limit=400"`
    //! @param options the option string.
    inline void options(const char *options);

    const std::string& get_options() const { return m_options; }

private:
    std::string m_options;
    std::string s_view;
    std::string s_design;
    friend class CallbackViewQuery;
    lcb_VIEWHANDLE vhptr;
    inline void add_cmd_flag(int flag, bool enabled);
};

class ViewRow {
public:
    //! Get the emitted key
    //! @return the emitted key as a string
    const Buffer& key() const { return m_key; }

    //! Get the emitted value
    //! @return the emitted value as a string
    const Buffer& value() const { return m_value; }

    //! Get the emitted geometry (only valid for GeoSpatial views)
    //! @return the GeoJSON, as a string
    const Buffer& geometry() const { return m_geometry; }

    //! Get the document ID for the item (not applicable for reduce)
    //! @return the document ID.
    const Buffer& docid() const { return m_docid; }

    //! Get the corresponding document.
    //! This returns a reference to the document. A document will only
    //! be present if ViewCommand::include_docs() was called with a
    //! true value. Additionally, the #has_document() method should be
    //! called to ensure the document is valid
    //! @return A reference to a document
    const GetResponse& document() const { return m_document; }

    inline void detatch();

    //! Indicates whether a GetResponse is available.
    //! @return true if there is a GetResponse
    //! @note a true return value does not indicate the contained document
    //! was fetched successfuly, just that it may be inspected.
    bool has_document() const { return m_hasdoc; }
private:
    inline char *detatch_buf(Buffer& tgt, char *tmp);
    ViewRow(Client&, const lcb_RESPVIEWQUERY *resp);

    std::shared_ptr<char> m_buf;
    Buffer m_key;
    Buffer m_value;
    Buffer m_geometry;
    Buffer m_docid;

    GetResponse m_document;
    bool m_hasdoc;
    friend class Client;
    friend class CallbackViewQuery;
};

class ViewMeta {
public:
    const std::string& body() const { return m_body; }
    const std::string& http_body() const { return m_http; }
    Status status() const { return m_rc; }
    short http_status() const { return m_htcode; }
    ViewMeta(){}
private:
    std::string m_body;
    std::string m_http;
    Status m_rc;
    short m_htcode = 0;
    friend class CallbackViewQuery;
    inline ViewMeta(const lcb_RESPVIEWQUERY *resp);
};


namespace Internal { class ViewIterator; }

// View query which can be used with a callback!
class CallbackViewQuery {
public:
    typedef std::function<void(ViewRow&&, CallbackViewQuery*)> RowCallback;
    typedef std::function<void(ViewMeta&&, CallbackViewQuery*)> DoneCallback;

    //! Initialize the query object.
    //! @param client the client on which to issue the query
    //! @param cmd a populated command object
    //! @param status used to indicate any errors during the construction
    //!        of the query. If status is false (i.e. failure), then this object
    //!        must not be used further.
    inline CallbackViewQuery(Client& client, const ViewCommand& cmd, Status& status,
        RowCallback rowcb = NULL, DoneCallback done_cb = NULL);

    inline virtual ~CallbackViewQuery();
    inline void _dispatch(const lcb_RESPVIEWQUERY *resp);

    //! @private

    //! Abort the query.
    //! This stops the query and ensures no more rows are fetched from the
    //! network.
    inline void stop();

    //! Whether this query object is still active (still has rows to be fetched)
    //! @return true if still active.
    bool active() const { return vh != NULL; }
protected:
    Client& cli;
private:
    CallbackViewQuery(CallbackViewQuery& other) = delete;
    CallbackViewQuery& operator=(const CallbackViewQuery& other) = delete;
    RowCallback m_rowcb = NULL;
    DoneCallback m_donecb = NULL;
    lcb_VIEWHANDLE vh = NULL;
};

//! This class may be used to execute a view query and iterate over its
//! results.
class ViewQuery
        : public CallbackViewQuery,
          protected Internal::RowProvider<ViewRow> {
public:
    inline ViewQuery(Client&, const ViewCommand&, Status&);

    //! Get the metadata for the query execution. This contains JSON metadata as
    //! well as the return code. This should only be called if `!active()`
    //! @return the view metadata
    const ViewMeta& meta() const { return m_meta; }
    typedef Internal::RowIterator<ViewRow> const_iterator;

    //! Begin iterating on the rows of the view. The returned
    //! iterator will incrementally fetch rows from the network
    //! until no more rows remain.
    inline const_iterator begin() { return rp_begin(); }
    inline const_iterator end() { return rp_end(); }
    inline Status status() const;

protected:
    bool rp_active() const override { return active(); }
    void rp_wait() override { cli.wait(); }

private:
    ViewMeta m_meta;
    void handle_row(ViewRow&&);
    void handle_done(ViewMeta&&);
};

namespace Internal {
extern "C" {
static void viewcb(lcb_t, int, const lcb_RESPVIEWQUERY *resp) {
    ViewQuery *vq = reinterpret_cast<ViewQuery*>(resp->cookie);
    vq->_dispatch(resp);
}
}
} // namespace Internal
} // namespace Couchbase

#include <libcouchbase/couchbase++/views.inl.h>

#endif
