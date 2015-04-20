#include <libcouchbase/couchbase++.h>
#include <libcouchbase/couchbase++/views.h>
#include <libcouchbase/couchbase++/endure.h>
#include <libcouchbase/couchbase++/logging.h>
#include <cassert>
#include <iostream>
#include <vector>

using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::map;
using std::vector;

using namespace Couchbase;

int main(int argc, char **argv)
{
    //! Connect to the client
    string connstr(argc > 1 ? argv[1] : "couchbase://localhost/default");
    Couchbase::Client h(connstr);
    Status rv = h.connect();
    if (!rv.success()) {
        cout << "Couldn't connect to '" << connstr << "'. "
                << "Reason: " << rv << endl;
        exit(EXIT_FAILURE);
    }


    auto sres = h.upsert("foo",
            "{ \"v\": 100.1, \"list\": [1,2,3,4,5,6,7], \"talk\": \"About Foo.\" }");
    cout << "Got status for store. Cas=" << std::hex << sres.cas() << endl;

    auto gres = h.get("foo");
    cout << "Got value: " << gres.value() << std::endl;

    gres = h.get("non-exist-key");
    cout << "Got status for non-existent key: " << gres.status() << endl;

    BatchCommand<UpsertCommand, StoreResponse> bulkStore(h);
    for (size_t ii = 0; ii < 10; ii++) {
        bulkStore.add("Key", "Value");
    }
    bulkStore.submit();
    h.wait();

    BatchCommand<GetCommand, GetResponse> bulkGet(h);
    for (size_t ii = 0; ii < 10; ii++) {
        bulkGet.add("Key");
    }
    bulkGet.submit();
    h.wait();

    CallbackCommand<GetCommand, GetResponse> cbGet(h, [](GetResponse& resp) {
        cout << "Got response for: " << resp.key() << endl;
        cout << "  Status: " << resp.status() << endl;
        cout << "  Value: " << resp.value() << endl;
    });

    cbGet.add("Key");
    cbGet.submit();
    h.wait();

    h.upsert("foo", "FOOVALUE");
    h.upsert("bar", "BARVALUE");
    h.upsert("baz", "BAZVALUE");

    //! Use durability requirements
    sres = h.upsert("toEndure", "toEndure");
    cout << "Endure Status: "
            << h.endure(EndureCommand("toEndure", sres.cas()),
                DurabilityOptions(PersistTo::MASTER)).status();

    //! Remove the items we just created
    h.remove("foo");
    h.remove("bar");
    h.remove("baz");

    Status status;
    ViewCommand vCmd("beer", "brewery_beers");
    vCmd.include_docs();
    vCmd.add_option("limit", 10);
    vCmd.add_option("skip", 10);
    vCmd.add_option("descending", true);
    cout << "using options: " << vCmd.get_options() << endl;

    ViewQuery query(h, vCmd, status);
    if (!status) {
        cerr << "Error with view command: " << status << endl;
    }

    size_t numRows = 0;
    for (auto ii = query.begin(); ii != query.end(); ii++) {
            cout << "Key: " << ii->key() << endl;
            cout << "Value: " << ii->value() << endl;
            cout << "DocID: " << ii->docid() << endl;

            if (ii->has_document()) {
                std::string value;
                ii->document().value(value);
                cout << "Document: " << value << endl;
            } else {
                cout << "NO DOCUMENT!" << endl;
            }
        numRows ++;
    }
    cout << "Got: " << std::dec << numRows << " rows" << endl;
    if (!query.status()) {
        cerr << "Problem with query: " << query.status() << endl;
    }
    return 0;
}
