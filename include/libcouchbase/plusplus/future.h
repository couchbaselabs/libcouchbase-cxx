#include <libcouchbase/plusplus/lcb++.h>

namespace Couchbase {
namespace Mt {
class Context;
class FutureHandler;

class Future : public OperationContext {
public:
    enum ResponseOwnership {
        RELEASE_RESPONSE,
        NEED_RESPONSE
    };

    Future(FutureHandler *, Context *);
    virtual ~Future();

    /**
     * Wait until the response for this 'future' object is available
     */
    const void *getResponse();
    lcb_error_t getStatus() const { return status; }

    /**
     * Indicate that the memory pointed to by this response is no longer
     * needed. This also indicates to the callback thread that it may
     * proceed.
     */

    void releaseResponse();

    /**
     * Reset this future object for subsequent use.
     */
    void reset();

    /**
     * Invoked when the callback is ready. If this function returns
     * NEED_RESPONSE then the IO thread will wait until the next call
     * to releaseResponse. Otherwise it is assumed the relevant data has been
     * copied over or otherwise utilized by the consumer.
     */
    virtual ResponseOwnership gotResponse(const ResponseBase *r, lcb_error_t e) {
        resp = r;
        status = e;
        return NEED_RESPONSE;
    }

    void waitToFreeResponse();

protected:
    const void * volatile resp;
    lcb_error_t status;

private:
    Context *mtCtx;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    friend class FutureHandler;

};

class FutureHandler : public ResponseHandler
{
public:
    void onDefault(OperationContext *, const ResponseBase *, lcb_error_t);
};

}
}
