#include <libcouchbase/plusplus/common.h>
#include <libcouchbase/plusplus/connection.h>

using std::string;
namespace Couchbase {

extern "C" {

#define OC(c) \
    const_cast<OperationContext *>( \
        reinterpret_cast<const OperationContext *>(c))


static void thunk_store(lcb_t, const void *cookie,
                        lcb_storage_t,
                        lcb_error_t status,
                        const lcb_store_resp_t *resp)
{
    StoreResponse sresp(resp);
    OperationContext *oc = OC(cookie);
    oc->handler->onStore(oc, &sresp, status);
}

static void thunk_onGet(lcb_t, const void *cookie,
                        lcb_error_t status, const lcb_get_resp_t *resp)
{
    GetResponse cppresp(resp);
    OperationContext *ctx = OC(cookie);
    ctx->handler->onGet(ctx, &cppresp, status);
}

static void thunk_onDelete(lcb_t, const void *cookie,
                           lcb_error_t status, const lcb_remove_resp_t *resp)
{
    DeleteResponse cppresp(resp);
    OperationContext *ctx = OC(cookie);
    ctx->handler->onDelete(ctx, &cppresp, status);
}

static void thunk_onArithmetic(lcb_t, const void *cookie,
                               lcb_error_t status, const lcb_arithmetic_resp_t *resp)
{
    ArithmeticResponse cppresp(resp);
    OperationContext *ctx = OC(cookie);
    ctx->handler->onArithmetic(ctx, &cppresp, status);
}

static void thunk_onTouch(lcb_t, const void *cookie,
                          lcb_error_t status, const lcb_touch_resp_t *resp)
{
    TouchResponse cppresp(resp);
    OperationContext *ctx = OC(cookie);
    ctx->handler->onTouch(ctx, &cppresp, status);
}

static void thunk_onUnlock(lcb_t, const void *cookie, lcb_error_t status,
                           const lcb_unlock_resp_t *resp)
{
    UnlockResponse cppresp(resp);
    OperationContext *ctx = OC(cookie);
    ctx->handler->onUnlock(ctx, &cppresp, status);
}

}

void Connection::initCallbacks()
{
    lcb_set_get_callback(instance, thunk_onGet);
    lcb_set_store_callback(instance, thunk_store);
    lcb_set_remove_callback(instance, thunk_onDelete);
    lcb_set_touch_callback(instance, thunk_onTouch);
    lcb_set_arithmetic_callback(instance, thunk_onArithmetic);
    lcb_set_unlock_callback(instance, thunk_onUnlock);
}


static void setIfEmpty(const std::string& src, const char **target)
{
    if (src.empty()) {
        *target = NULL;
    }
    *target = src.c_str();
}

lcb_error_t LcbFactory::createInstance(lcb_t *instance)
{
    *instance = NULL;
    struct lcb_create_st crOpts;
    memset(&crOpts, 0, sizeof(crOpts));

    crOpts.version = 1;

    std::string hostString;
    for (size_t ii = 0; ii < hosts.size(); ii++) {
        hostString += hosts[ii];
        hostString += ';';
    }

    setIfEmpty(hostString, &crOpts.v.v1.host);
    setIfEmpty(bucket, &crOpts.v.v1.bucket);
    setIfEmpty(username, &crOpts.v.v1.user);
    setIfEmpty(passwd, &crOpts.v.v1.passwd);
    crOpts.v.v1.io = io;

    return lcb_create(instance, &crOpts);
}

Connection::Connection(lcb_error_t &status, LcbFactory &params) : instance(NULL)
{
    status = params.createInstance(&instance);
    if (status == LCB_SUCCESS) {
        initCallbacks();
        lcb_set_cookie(instance, this);
    }
}

}
