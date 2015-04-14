#ifndef LCB_PLUSPLUS_H
#error "Include <libcouchbase/couchbase++.h> first!"
#endif

#ifndef LCB_PLUSPLUS_VIEWS_H
#define LCB_PLUSPLUS_VIEWS_H

#include <deque>

namespace Couchbase {

class ViewQuery;

namespace Internal {
extern "C" { static void viewcb(lcb_t,int,const lcb_RESPVIEWQUERY*); }
}

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

    //! Set the raw option string, e.g. `"stale=false&limit=400"`
    //! @param options the option string.
    inline void options(const char *options);

    const std::string& get_options() const { return m_options; }

private:
    std::string m_options;
    std::string s_view;
    std::string s_design;
    friend class ViewQuery;
    lcb_VIEWHANDLE vhptr;
    inline void add_cmd_flag(int flag, bool enabled);
};

class ViewRow {
public:
    //! Get the emitted key
    //! @return the emitted key as a string
    const std::string& key() const { return m_key; }

    //! Get the emitted value
    //! @return the emitted value as a string
    const std::string& value() const { return m_value; }

    //! Get the emitted geometry (only valid for GeoSpatial views)
    //! @return the GeoJSON, as a string
    const std::string& geometry() const { return m_geometry; }

    //! Get the document ID for the item (not applicable for reduce)
    //! @return the document ID.
    const std::string& docid() const { return m_docid; }

    //! Get the corresponding document.
    //! This returns a reference to the document. A document will only
    //! be present if ViewCommand::include_docs() was called with a
    //! true value. Additionally, the #has_document() method should be
    //! called to ensure the document is valid
    //! @return A reference to a document
    const GetResponse& document() const { return m_document; }

    //! Indicates whether a GetResponse is available.
    //! @return true if there is a GetResponse
    //! @note a true return value does not indicate the contained document
    //! was fetched successfuly, just that it may be inspected.
    bool has_document() const { return m_hasdoc; }

private:
    inline static void f2s(const void*, size_t, std::string&);
    ViewRow(const lcb_RESPVIEWQUERY *resp);

    std::string m_key;
    std::string m_value;
    std::string m_geometry;
    std::string m_id;
    std::string m_docid;
    GetResponse m_document;
    bool m_hasdoc;
    friend class Client;
    friend class ViewQuery;
};

class ViewMeta {
public:
    std::string body;
    std::string http;
    Status rc;
    short htcode;
private:
    friend class ViewQuery;
    inline ViewMeta(const lcb_RESPVIEWQUERY *resp);
};


namespace Internal { class ViewIterator; }

//! This class may be used to execute a view query and iterate over its
//! results.
class ViewQuery {
public:
    //! Initialize the query object.
    //! @param client the client on which to issue the query
    //! @param cmd a populated command object
    //! @param status used to indicate any errors during the construction
    //!        of the query. If status is false (i.e. failure), then this object
    //!        must not be used further.
    inline ViewQuery(Client& client, const ViewCommand& cmd, Status& status);
    inline ~ViewQuery();
    inline void _dispatch(const lcb_RESPVIEWQUERY *resp);


    //! @private

    //! Abort the query.
    //! This stops the query and ensures no more rows are fetched from the
    //! network.
    inline void stop();

    //! Whether this query object is still active (still has rows to be fetched)
    //! @return true if still active.
    bool active() const { return vh != NULL; }

    //! Get the metadata for the query execution. This contains JSON metadata as
    //! well as the return code. This should only be called if `!active()`
    //! @return a pointer to the view metadata
    const ViewMeta* meta() const { return m_meta; }

    typedef Internal::ViewIterator const_iterator;

    //! Begin iterating on the rows of the view. The returned
    //! iterator will incrementally fetch rows from the network
    //! until no more rows remain.
    inline const_iterator begin();
    inline const_iterator end();
    inline Status status() const;

private:
    Client& cli;
    ViewMeta *m_meta;
    lcb_VIEWHANDLE vh;
    std::deque<ViewRow> rows;
    friend class Internal::ViewIterator;
    inline const ViewRow* next();
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
