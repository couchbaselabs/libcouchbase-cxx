/**
 * Libcouchbase multi threaded extensions:
 *
 * These extensions layer on top of libcouchbase and allow scheduling of
 * operations for libcouchbase in a multi threaded manner.
 *
 * The design is as follows:
 *
 * Individual threads will assume a blocking pattern for operations (though
 * this is not strictly required -- it _may_ be callback oriented). In such
 * a scenario, the event loop will run in a dedicated thread (effectively
 * as a daemon).
 *
 * The ability to schedule new operations will depend on the event loop
 * being ready to receive such events. To such measure, other threads will
 * only be able to schedule operations when:
 *
 * 1) The event loop is suspended (i.e. lcb_wait has returned)
 * 2) The event loop is in the middle of a callback
 *
 * Since we cannot ensure either (1) or (2) in a deterministic fashion, a
 * new server socket shall be created (in the mt layer). A single byte shall
 * be written to the socket - which in turn shall invoke the watcher, which
 * in turn will unlock the mutex needed for other threads to schedule their
 * calls. Note that to ensure that only one byte is written, the
 * 'socket' context itself will need to be locked as well. We'll want to
 * enable TCP_NDELAY as well so that there's no delay when writing a single
 * byte.
 *
 * Notification of the response can take place in one of three manners:
 * 1) The cookie (or OperationContext) contains a callback in itself at
 *    which point it is invoked and the callback proceeds
 *
 * 2) The cookie object contains a waitable mutex and/or condition variable.
 *    It is assumed the calling thread is waiting on the cookie to be ready
 *    and when the response handler receives the cookie, it is set to the
 *    response object (from lcb); the mutex unlocked, and the waiting thread
 *    signalled.
 *
 *    At this point the wait semantics are reversed, and the callback then
 *    enters a wait state until the recipient thread is done with the response.
 *
 *    It is then up to the waiting thread to 'release' this response - at which
 *    point the callback thread is signalled and continues.
 *
 * 3) The response' contents are copied.
 *
 * Note that all these options may exist within the same implementation - and it
 * is up to the user to decide what form has the best performance for the
 * application.
 */
#include <libcouchbase/plusplus/lcb++.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>

namespace Couchbase {
namespace Mt {

using namespace Couchbase;

class Context {
public:
    Context(Connection *, lcb_io_opt_t);
    ~Context();
    /**
     * Starts the 'context'. This does the following:
     *
     * 1) Establishes the listening socket
     * 2) Spins up a new thread which does:
     *  I. Accept the new socket for the client
     *  II. Sets up a 'read event' for the socket
     *  III. Releases locks once the read event has been established
     * 3) Connects to the socket (from the client side).
     * 4) Connects the connection object.
     */
    bool start();

    /**
     * Call these methods before and after requesting libcouchbase
     * operations. Internally, the start method will wait until the 'daemon'
     * thread is either suspended or inside one of the handlers. The daemon
     * io thread will not proceed until 'stopSchedulePipeline' is called.
     */
    void startSchedulePipeline();
    void endSchedulePipeline();

    /**
     * Inverse of the pipeline methods; these are called by the io callbacks
     * when entering and exiting that function.
     */
    void handlerEnter();
    void handlerLeave();

    /**
     * Helper function to start the libcouchbase loop in its own context.
     */
    void runDaemon();

    /**
     * Helper function to notify that a new configuration has been received
     */
    void setConnected();
private:
    // Listen here
    int lsnSock;
    // Client socket used. The client side writes to this connection,
    // and lcb has a read event established for it.
    int acceptedSocket;
    int connectedSocket;
    struct sockaddr_in addr;
    void *event;
    pthread_t daemonThread;

    // Mutex protecting the actual lcb_t
    pthread_mutex_t connobjMutex;
    pthread_mutex_t eventMutex;
    Connection *conn;
    lcb_io_opt_t io;
    bool lcbConnected;
    volatile unsigned int waiters;

    inline bool setupListeningSocket();
    inline bool waitForClientConnection();
    inline void runOnce();
    inline void notifySocket();
    static inline bool initSockCommon(int);
    void incrementWaiters() { ++waiters; }
};

class MtConnection : public Connection {
public:
    MtConnection(lcb_error_t&, LcbFactory &);
    bool connectSync();
    void onConnected();
    void lockConnection() {
        pthread_mutex_lock(&mutex);
    }
    void unlockConnection() {
        pthread_mutex_unlock(&mutex);
    }
private:
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    bool connected;
};

}
}
