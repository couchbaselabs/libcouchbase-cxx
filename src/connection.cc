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

Connection::Connection(lcb_error_t &status,
                       const string &host,
                       const string &bkt,
                       const string &user,
                       const string &pass,
                       lcb_io_opt_t io)
: instance(NULL), hostname(host), bucket(bkt), username(user), password(pass)
{
    memset(&creationOptions, 0, sizeof(creationOptions));
    creationOptions.version = 1;

    if (!hostname.empty()) {
        creationOptions.v.v1.host = hostname.c_str();
    }

    if (!bucket.empty()) {
        creationOptions.v.v1.bucket = bucket.c_str();
    }

    if (!username.empty()) {
        creationOptions.v.v1.user = username.c_str();
    }

    if (!password.empty()) {
        creationOptions.v.v1.passwd = password.c_str();
    }

    if (io) {
        creationOptions.v.v1.io = io;
    }

    status = lcb_create(&instance, &creationOptions);
    if (status == LCB_SUCCESS) {
        initCallbacks();
        lcb_set_cookie(instance, this);
    }
}

}
