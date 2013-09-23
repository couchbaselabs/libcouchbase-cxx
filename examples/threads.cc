#include <libcouchbase/plusplus/lcb++.h>
#include <libcouchbase/plusplus/mt.h>
#include <libcouchbase/plusplus/future.h>
#include <pthread.h>
#include <cassert>
#include <cstdio>

using namespace Couchbase;
using namespace Couchbase::Mt;

class MyInfo {
public:
    MtConnection *conn;
    FutureHandler *handler;
    Context *ctx;
};

extern "C" {
static void *thr_run(void *arg) {
    MyInfo *info = reinterpret_cast<MyInfo*>(arg);
    MtConnection *conn = info->conn;
    Context *ctx = info->ctx;
    Future f(info->handler, ctx);

    ctx->startSchedulePipeline();
    lcb_error_t err = conn->schedule(StoreCommand("foo", "bar"), &f);
    ctx->endSchedulePipeline();
    assert(err == LCB_SUCCESS);

    const void *gresp = f.getResponse();
    const StoreResponse *r = reinterpret_cast<const StoreResponse *>(gresp);
    assert(r->getKey().size());
    assert(f.getStatus() == LCB_SUCCESS);
    f.releaseResponse();
    f.reset();

    while (1) {
        ctx->startSchedulePipeline();
        err = conn->schedule(StoreCommand("foo", "bar"), &f);
        ctx->endSchedulePipeline();
        assert(err == LCB_SUCCESS);

        gresp = f.getResponse();
        r = reinterpret_cast<const StoreResponse *>(gresp);
        assert(f.getStatus() == LCB_SUCCESS);
        f.reset();
        f.releaseResponse();
    }
    return NULL;
}
}

int main(void) {
    lcb_io_opt_t io;
    lcb_create_io_ops(&io, NULL);
    assert(io);
    lcb_error_t status;
    LcbFactory factory;
    factory.io = io;


    MtConnection conn(status, factory);
    assert(status == LCB_SUCCESS);
    FutureHandler handler;
    Context ctx(&conn, io);
    ctx.start();

    conn.connectSync();
    printf("Connected!!\n");

    MyInfo info;
    info.conn = &conn;
    info.ctx = &ctx;
    info.handler = &handler;

    int thr_max = 5;
    pthread_t thrs[thr_max];
    for (int ii = 0; ii < thr_max; ii++) {
        pthread_create(thrs + ii, NULL, thr_run, &info);
    }

    for (int ii =0; ii < thr_max; ii++) {
        void *arg;
        pthread_join(thrs[ii], &arg);
    }
    return 0;
}
