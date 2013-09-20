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
    }

    void onStore(OperationContext *ctx, const StoreResponse *resp, lcb_error_t err)
    {
        assert(err == LCB_SUCCESS);
        cout << "Stored key: " << resp->getKey() << endl;
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

    std::string key = "I am a key";
    std::string value = "I am a value";

    StoreCommand scmd;
    scmd.setKey(key);
    scmd.setValue(value);
    scmd.setMode(LCB_SET);

    ctx.handler = &handler;
    status = conn.scheduleStore(&scmd, &ctx);
    assert(status == LCB_SUCCESS);

    GetCommand gcmd;
    gcmd.setKey(key);
    status = conn.scheduleGet(&gcmd, &ctx);
    assert(status == LCB_SUCCESS);
    conn.wait();

    return 0;
}
