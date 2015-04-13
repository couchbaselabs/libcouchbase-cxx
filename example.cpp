#include <libcouchbase/couchbase++.h>
#include <libcouchbase/couchbase++/views.h>
#include <libcouchbase/couchbase++/endure.h>
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
    const char *connstr = argc > 1 ? argv[1] : "couchbase://localhost/default";
    Couchbase::Client h(connstr);
    Status rv = h.connect();
    if (rv.errcode() != LCB_SUCCESS) {
        cout << "Something wrong to connect '" << connstr << "'" << connstr;
        return -1;
    }

    //! Store item.
    UpsertOperation scmd("foo",
        "{ \"v\": 100.1, \"list\": [1,2,3,4,5,6,7], \"talk\": \"About Foo.\" }");
    Response& sres = scmd.run(h);
    cout << "Got status for store. Cas=" << std::hex << sres.cas() << endl;

    //! Use a command to retrieve an item
    GetOperation cmd("foo");
    cmd.run(h);
    //! The response object can be returned via run() or via response()
    GetResponse& res = cmd.response();
    cout << "Got value: " << res.value() << std::endl;

    cmd.key("non-exist-key");
    cmd.run(h);
    cout << "Got status for non-existent key: " << cmd.response().status() << endl;

    //! Use batch contexts to perform "bulk" operations
    BatchContext ctx(h);
    for (size_t ii = 0; ii < 10; ii++) {
        scmd.schedule(ctx);
    }
    ctx.submit();
    h.wait();

    //! Store a bunch of items, one at a time (does not use batching)
    UpsertOperation("foo", "FOOVALUE").run(h);
    UpsertOperation("bar", "BARVALUE").run(h);
    UpsertOperation("baz", "BAZVALUE").run(h);

    //! Use durability requirements
    UpsertOperation endureOp("toEndure", "toEndure");
    endureOp.run(h);
    cout << "Endure status: " << DurabilityOperation(endureOp).run(h).status() << endl;

    //! Reset the context.
    ctx.reset();

    //! Use shorthand for "getting" an item.
    ctx.get("foo");
    ctx.get("bar");
    ctx.get("baz");
    ctx.submit(); // Schedule the pipeline

    // Wait for them all!
    h.wait();

    cout << "Value for foo is " << ctx["foo"].value() << endl;
    cout << "Value for bar is " << ctx["bar"].value() << endl;
    cout << "Value for baz is " << ctx["baz"].value() << endl;

    //! Remove the items we just created
    RemoveOperation("foo").run(h);
    RemoveOperation("bar").run(h);
    RemoveOperation("baz").run(h);

    for (int ii = 0; ii < 1; ii++) {
        Status status;
        ViewCommand vCmd("beer", "brewery_beers");
        vCmd.include_docs();
        vCmd.add_option("limit", 100);
        vCmd.add_option("skip", 10);
        vCmd.add_option("descending", true);
        cout << "using options: " << vCmd.get_options() << endl;

        ViewQuery query(h, vCmd, status);
        if (!status) {
            cerr << "Error with view command: " << status << endl;
        }

        size_t numRows = 0;
        for (ViewQuery::const_iterator ii = query.begin(); ii != query.end(); ii++) {
//            const ViewRow& rr = *ii;
//            cout << "Key: " << rr.key() << endl;
//            cout << "Value: " << rr.value() << endl;
//            cout << "DocID: " << rr.docid() << endl;
//
//            if (rr.has_document()) {
//                std::string value;
//                rr.document().value(value);
//                cout << "Document: " << value << endl;
//            } else {
//                cout << "NO DOCUMENT!" << endl;
//            }
            numRows ++;
        }
        cout << "Got: " << std::dec << numRows << " rows" << endl;
        if (!query.status()) {
            cerr << "Problem with query: " << query.status() << endl;
        }
    }
    return 0;
}
