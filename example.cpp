#include <libcouchbase/lcb++.h>
#include <cassert>
#include <iostream>
#include <vector>

using std::cout;
using std::endl;
using std::string;
using std::map;
using std::vector;

using namespace Couchbase;

int main(void)
{
    const char* url = "couchbase://localhost/default";
    Couchbase::Client h(url);
    Status rv = h.connect();
    if (rv.errcode() != LCB_SUCCESS) {
        cout << "Something wrong to connect '" << url << "'" << endl;
        return -1;
    }

    // Use a command to store an item
    StoreOperation scmd(LCB_SET);
    scmd.key("Foo");
    scmd.value("{ \"v\": 100.1, \"list\": [1,2,3,4,5,6,7], \"talk\": \"About Foo.\" }");
    scmd.run(h);
    Response& sres = scmd.response();
    cout << "Got status for store. Cas=" << std::hex << sres.cas() << endl;

    // Use a command to retrieve an item
    GetOperation cmd("Foo");
    cmd.run(h);
    GetResponse& res = cmd.response();
    cout << "Got value: " << res.value() << std::endl;

    // Scheduling context. Schedule multiple operations
    BatchContext ctx(h);
    for (size_t ii = 0; ii < 100000; ii++) {
        scmd.schedule(ctx);
    }
    ctx.submit();
    h.wait();

    // These block
    h.store("foo", "FOOVALUE");
    h.store("bar", "BARVALUE");
    h.store("baz", "BAZVALUE");

    ctx.reset();
    ctx.get("foo");
    ctx.get("bar");
    ctx.get("baz");
    ctx.submit(); // Schedule the pipeline

    // Wait for them all!
    h.wait();

    cout << "Value for foo is " << ctx["foo"].value() << endl;
    cout << "Value for bar is " << ctx["bar"].value() << endl;
    cout << "Value for baz is " << ctx["baz"].value() << endl;

    return 0;
}
