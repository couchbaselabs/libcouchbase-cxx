#include <libcouchbase/plusplus/mt.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <cassert>
#include <cstdio>

using namespace Couchbase;
using namespace Couchbase::Mt;

Context::Context(Connection *c, lcb_io_opt_t iops)
: lsnSock(-1),
  acceptedSocket(-1),
  connectedSocket(-1),
  event(NULL),
  conn(c),
  io(iops),
  waiters(0)
{
    pthread_mutex_init(&eventMutex, NULL);
    pthread_mutex_init(&connobjMutex, NULL);
    memset(&daemonThread, 0, sizeof(daemonThread));
}

Context::~Context() {
    pthread_mutex_destroy(&eventMutex);
    pthread_mutex_destroy(&connobjMutex);
}

bool Context::setupListeningSocket()
{
    if (io->version != 0) {
        return false; // only v0 supported currently.
    }

    memset(&addr, 0, sizeof(addr));
    lsnSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (lsnSock < 0) {
        return false;
    }

    if (bind(lsnSock, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        return false;
    }
    if (listen(lsnSock, 5) < 0) {
        return false;
    }

    socklen_t addrsz = sizeof(addr);
    if (getsockname(lsnSock, (struct sockaddr *)&addr, &addrsz) != 0) {
        return false;
    }
    return true;
}

extern "C" {
static void read_handler(lcb_socket_t s, short which, void *arg)
{
    int buf[4096];
    while (recv(s, &buf, sizeof(buf), 0) == sizeof(buf)) {
        // no body
    }

    Context *ctx = reinterpret_cast<Context *>(arg);
    ctx->handlerEnter();
    ctx->handlerLeave();
    (void)which;
}
}

void Context::notifySocket()
{
    char c = '*';
    send(connectedSocket, &c, sizeof(c), 0);
}

bool Context::waitForClientConnection() {
    struct sockaddr_in clisaddr;
    socklen_t addrsz = sizeof(clisaddr);
    memset(&clisaddr, 0, sizeof(clisaddr));
    acceptedSocket = accept(lsnSock, (struct sockaddr *)&clisaddr, &addrsz);
    if (acceptedSocket == -1) {
        return false;
    }
    return initSockCommon(acceptedSocket);
}

void Context::startSchedulePipeline() {
    pthread_mutex_lock(&connobjMutex);
    waiters++;
    int rv = pthread_mutex_trylock(&eventMutex);
    if (rv) {
        notifySocket();
        pthread_mutex_lock(&eventMutex);
    }

//    printf("Got event mutex!\n");
    waiters--;
}

void Context::endSchedulePipeline() {
    pthread_mutex_unlock(&connobjMutex);
    pthread_mutex_unlock(&eventMutex);
}

void Context::handlerEnter() {
    int rv = pthread_mutex_unlock(&eventMutex);
    assert(!rv);
}

void Context::handlerLeave() {
    while (waiters) {
//        printf("Waiters: %d\n", waiters);
    }
    pthread_mutex_lock(&eventMutex);
//    printf("Returning..\n");
}

extern "C" {
static void *notifier_run(void *arg) {
    Context *ctx = reinterpret_cast<Context *>(arg);
    ctx->runDaemon();
    return NULL;
}
}

void Context::runDaemon() {
    pthread_mutex_lock(&eventMutex);
    // Try to set up the listening socket
    waitForClientConnection();

    event = io->v.v0.create_event(io);
    io->v.v0.update_event(io,
                          acceptedSocket,
                          event,
                          LCB_READ_EVENT,
                          this,
                          read_handler);

    io->v.v0.run_event_loop(io);
    pthread_mutex_unlock(&eventMutex);
}

bool Context::start()
{
    if (!setupListeningSocket()) {
        return false;
    }

    pthread_create(&daemonThread, NULL, notifier_run, this);
    connectedSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (connectedSocket == -1) {
        return false;
    }


    int rv = connect(connectedSocket, (struct sockaddr *)&addr, sizeof(addr));
    if (rv != 0) {
        return false;
    }

    return initSockCommon(connectedSocket);
}

bool Context::initSockCommon(int sock)
{
    // Set it to non-blocking
    int rv = fcntl(sock, F_GETFL);
    if (rv == -1) {
        return false;
    }
    rv = fcntl(sock, F_SETFL, rv | O_NONBLOCK);
    if (rv == -1) {
        return false;
    }

    rv = 1;
    rv = setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &rv, sizeof(rv));
    return rv == 0;
}


extern "C" {
static void config_callback(lcb_t instance, lcb_configuration_t)
{
    MtConnection *conn = reinterpret_cast<MtConnection *>(
            const_cast<void*>(lcb_get_cookie(instance)));
    conn->onConnected();
}
}
// Connection stuff
MtConnection::MtConnection(lcb_error_t &err, LcbFactory &params)
: Connection(err, params), connected(false)
{
    if (err != LCB_SUCCESS) {
        return;
    }
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&cond, NULL);
}

bool MtConnection::connectSync()
{
    assert(!connected);
    pthread_mutex_lock(&mutex);
    lcb_set_configuration_callback(instance, config_callback);

    if (this->connect() != LCB_SUCCESS) {
        pthread_mutex_unlock(&mutex);
        return false;
    }

    while (!connected) {
        pthread_cond_wait(&cond, &mutex);
    }
    pthread_mutex_unlock(&mutex);
    return true;
}

void MtConnection::onConnected() {
    lcb_set_configuration_callback(instance, NULL);
    pthread_mutex_lock(&mutex);
    connected = true;
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&mutex);
}
