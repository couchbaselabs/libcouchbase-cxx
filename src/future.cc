#include <libcouchbase/plusplus/mt.h>
#include <libcouchbase/plusplus/future.h>
#include <cstdio>


using namespace Couchbase;
using namespace Couchbase::Mt;

Future::Future(FutureHandler *h, Context *ctx)
: OperationContext(), mtCtx(ctx)
{
    this->handler = h;
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&cond, NULL);
}


Future::~Future()
{
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&cond);
}

void Future::reset()
{
    resp = NULL;
    status = LCB_SUCCESS;
}

const void* Future::getResponse()
{
    int rv = pthread_mutex_lock(&mutex);
    assert(!rv);

    while (!resp) {
        rv = pthread_cond_wait(&cond, &mutex);
        assert(!rv);
    }

    return resp;
}

void Future::releaseResponse()
{
    resp = NULL;
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&mutex);
}

void Future::waitToFreeResponse()
{
    pthread_mutex_lock(&mutex);
    while (resp) {
        int rv = pthread_cond_wait(&cond, &mutex);
        assert(!rv);
    }

    assert(!resp);
    pthread_mutex_unlock(&mutex);
}


// Operation Handler
void FutureHandler::onDefault(OperationContext *ctx,
                              const ResponseBase *resp, lcb_error_t err)
{
    Future *ft = reinterpret_cast<Future*>(ctx);
    ft->mtCtx->handlerEnter();

    pthread_mutex_lock(&ft->mutex);

    ft->resp = resp;
    ft->status = err;

    pthread_cond_signal(&ft->cond);
    pthread_mutex_unlock(&ft->mutex);

    ft->waitToFreeResponse();

    ft->mtCtx->handlerLeave();
}
