#include <libcouchbase/plusplus/lcb++.h>
#include <libcouchbase/plusplus/mt.h>
#include <libcouchbase/plusplus/future.h>
#include <pthread.h>
#include <cassert>
#include <cstdio>
#include <unistd.h>
#include <iostream>
#include "cliopts.h"

using namespace Couchbase;
using namespace Couchbase::Mt;
using std::string;
using std::cout;
using std::endl;

struct Settings {
    int nThreads;
    int batchSize;
};

static Settings globalSettings;

static volatile unsigned long opCounter = 0;
static time_t beginTime;

class MyInfo {
public:
    MtConnection *conn;
    FutureHandler *handler;
    Context *ctx;
    std::string key;
    std::string value;
};

extern "C" {
static void *thr_run(void *arg) {
    MyInfo *info = reinterpret_cast<MyInfo*>(arg);
    MtConnection *conn = info->conn;
    Context *ctx = info->ctx;

    Future f(info->handler, ctx);
    StoreCommand scmd(info->key, info->value);
    lcb_error_t err;

    int ncmds = globalSettings.batchSize;
    StoreCommand *cmdList[ncmds];
    for (int ii = 0; ii < ncmds; ii++) {
        cmdList[ii] = &scmd;
    }

    while (1) {
        ctx->startSchedulePipeline();
        err = conn->schedule(cmdList, ncmds, &f);
        ctx->endSchedulePipeline();
        assert(err == LCB_SUCCESS);

        for (int ii = 0; ii < ncmds; ii++) {
            const StoreResponse *r = f.get<StoreResponse>();
            assert(f.getStatus() == LCB_SUCCESS);
            assert(r->getCas());
            assert(r->getKey().size());
            f.releaseResponse();
            opCounter++;

        }
    }
    return NULL;
}
}

static void *op_printer(void *) {
    while (true) {
        time_t now = time(NULL);
        time_t duration = now - beginTime;
        if (duration) {
            float opsSec = opCounter / (now - beginTime);
            printf("Ops/sec: %0.2f    \r", opsSec);
            fflush(stdout);
        }
        sleep(1);
    }

    return NULL;
}



int main(int argc, char **argv) {
    beginTime = time(NULL);
    pthread_t opthr;

    const char *sHost = "localhost:8091";
    const char *sBucket = "default";
    globalSettings.batchSize = 1;
    globalSettings.nThreads = 4;
    LcbFactory factory;


    cliopts_entry optionEntries[] = {
        { 'H', "host", CLIOPTS_ARGT_STRING, &sHost },
        { 'b', "bucket", CLIOPTS_ARGT_STRING, &sBucket },
        { 't', "threads", CLIOPTS_ARGT_INT, &globalSettings.nThreads },
        { 's', "sched", CLIOPTS_ARGT_INT, &globalSettings.batchSize },
        { 0, NULL }
    };

    int lastix;
    if (-1 == cliopts_parse_options(optionEntries, argc, argv, &lastix, NULL)) {
        return 1;
    }

    factory.bucket = sBucket;
    factory.hosts.push_back(sHost);



    pthread_create(&opthr, NULL, op_printer, NULL);

    lcb_io_opt_t io;
    lcb_create_io_ops(&io, NULL);
    assert(io);
    lcb_error_t status;
    factory.io = io;

    MtConnection conn(status, factory);
    assert(status == LCB_SUCCESS);
    FutureHandler handler;
    Context ctx(&conn, io);
    conn.connectSync();

    ctx.start();
    printf("Connected!!\n");


    MyInfo infos[globalSettings.nThreads];
    pthread_t thrs[globalSettings.nThreads];
    for (int ii = 0; ii < globalSettings.nThreads; ii++) {
        MyInfo *info = infos + ii;
        info->conn = &conn;
        info->ctx = &ctx;
        info->handler = &handler;
        info->key = "thrkey_";
        info->key += ii;
        info->value = "thrval";

        pthread_create(thrs + ii, NULL, thr_run, info);
    }

    for (int ii =0; ii < globalSettings.nThreads; ii++) {
        void *arg;
        pthread_join(thrs[ii], &arg);
    }
    return 0;
}
