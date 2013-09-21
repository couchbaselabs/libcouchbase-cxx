#include <libcouchbase/plusplus/lcb++.h>
#include <cassert>
#include <iostream>

using namespace Couchbase;
using std::cout;
using std::cerr;
using std::endl;

class MyResponseHandler : public ResponseHandler
{
public:
    void onGet(OperationContext *ctx, const GetResponse *resp, lcb_error_t err)
    {
        assert(err == LCB_SUCCESS);
        cout << "Got response for key: " << resp->getKey() << endl;
        cout << "   " << resp->getValue() << endl;

        DeleteCommand dcmd;
        dcmd.initFromResponse(*resp);
        ctx->conn->schedule(dcmd, ctx);
    }

    void onStore(OperationContext *ctx, const StoreResponse *resp, lcb_error_t err)
    {
        assert(err == LCB_SUCCESS);
        cout << "Stored key: " << resp->getKey() << endl;
        cout << "   CAS: " << std::hex << resp->getCas() << endl;
    }

    void onDelete(OperationContext *ctx, const DeleteResponse *resp, lcb_error_t err)
    {
        assert(err == LCB_SUCCESS);
        cout << "Deleted key: " << resp->getKey() << endl;
        cout << "   CAS: " << std::hex << resp->getCas() << endl;
    }
};


int main(void)
{
    lcb_error_t status;
    Connection conn(status);
    MyResponseHandler handler;
    OperationContext ctx;
    assert(status == LCB_SUCCESS);

    assert((status = conn.connect()) == LCB_SUCCESS);
    conn.wait();

    ctx.handler = &handler;
    ctx.conn = &conn;

    std::string key = "I am a key";
    std::string value = "I am a value";

    status = conn.schedule(StoreCommand(key, value), &ctx);
    assert(status == LCB_SUCCESS);

    status = conn.schedule(GetCommand(key), &ctx);
    assert(status == LCB_SUCCESS);

    conn.wait();

    return 0;
}
