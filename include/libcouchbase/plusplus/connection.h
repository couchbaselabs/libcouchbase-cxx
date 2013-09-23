#ifndef LCB_CPP_CONNECTION_H
#define LCB_CPP_CONNECTION_H

#include "common.h"
#include "commands.h"
#include "responses.h"
#include <vector>

namespace Couchbase {

class LcbFactory {
public:
    LcbFactory(std::string hostname = "", std::string bucketname = "") : io(NULL) {
        if (!hostname.empty()) {
            hosts.push_back(hostname);
        }
        bucket = bucketname;
    }

    std::vector<std::string> hosts;
    std::string bucket;
    std::string username;
    std::string passwd;
    lcb_io_opt_t io;

    lcb_error_t createInstance(lcb_t *instance);
};

class Connection
{

public:
    Connection(lcb_error_t &status, LcbFactory &params);
    Connection(lcb_t existing, ResponseHandler *resphandler): instance(existing) {}

    virtual ~Connection() {
        if (instance) {
            lcb_destroy(instance);
        }
        instance = NULL;
    }

    lcb_t getInstance() {
        return instance;
    }

    lcb_error_t connect() {
        return lcb_connect(instance);
    }

    lcb_error_t wait() {
        return lcb_wait(instance);
    }


    // Get
    lcb_error_t schedule(const GetCommand * const *commands, size_t n, OperationContext *ctx) {
        return scheduleMulti<C_GetCmd>(ctx, commands, n, lcb_get);
    }

    lcb_error_t schedule(const GetCommand& cmd, OperationContext *ctx) {
        return scheduleSingle<C_GetCmd>(ctx, cmd, lcb_get);
    }

    // Store
    lcb_error_t schedule(const StoreCommand * const *commands, size_t n, OperationContext *ctx) {
        return scheduleMulti<C_StoreCmd>(ctx, commands, n, lcb_store);
    }

    lcb_error_t schedule(const StoreCommand &cmd, OperationContext *ctx) {
        return scheduleSingle<C_StoreCmd>(ctx, cmd, lcb_store);
    }

    // Delete
    lcb_error_t schedule(const DeleteCommand * const *commands, size_t n, OperationContext *ctx) {
        return scheduleMulti<C_RemoveCmd>(ctx, commands, n, lcb_remove);
    }

    lcb_error_t schedule(const DeleteCommand &cmd, OperationContext *ctx) {
        return scheduleSingle<C_RemoveCmd>(ctx, cmd, lcb_remove);
    }

    // Touch
    lcb_error_t schedule(const TouchCommand * const *commands, size_t n, OperationContext *ctx) {
        return scheduleMulti<C_TouchCmd>(ctx, commands, n, lcb_touch);
    }

    lcb_error_t schedule(const TouchCommand& cmd, OperationContext *ctx) {
        return scheduleSingle<C_TouchCmd>(ctx, cmd, lcb_touch);
    }

    // Arithmetic
    lcb_error_t schedule(const ArithmeticCommand * const *cmds, size_t n, OperationContext *ctx) {
        return scheduleMulti<C_ArithCmd>(ctx, cmds, n, lcb_arithmetic);
    }

    lcb_error_t schedule(const ArithmeticCommand &cmd, OperationContext *ctx) {
        return scheduleSingle<C_ArithCmd>(ctx, cmd, lcb_arithmetic);
    }

    // Unlock
    lcb_error_t schedule(const UnlockCommand * const * cmds, size_t n, OperationContext *ctx) {
        return scheduleMulti<C_UnlockCmd>(ctx, cmds, n, lcb_unlock);
    }

    lcb_error_t schedule(const UnlockCommand &cmd, OperationContext *ctx) {
        return scheduleSingle<C_UnlockCmd>(ctx, cmd, lcb_unlock);
    }

protected:
    lcb_t instance;

private:
    Connection(Connection&);
    void initCallbacks();


    template <typename T>
    lcb_error_t scheduleSingle(OperationContext *ctx, const KeyCommand& cmd,
                               lcb_error_t (*fn)(lcb_t, const void *, size_t, const T * const *)) {
        const T* p = (const T *)cmd.getLcbPointer();
        return fn(instance, ctx, 1, &p);
    }

    template <typename T, class C>
    lcb_error_t scheduleMulti(OperationContext *ctx, const C * const * cmds, size_t n,
                              lcb_error_t (*fn)(lcb_t, const void *, size_t, const T* const *)) {
        if (n == 1) {
            return scheduleSingle(ctx, *cmds[0], fn);
        }

        std::vector<const T*> l;
        l.reserve(n);
        for (size_t ii = 0; ii < n; ii++) {
            l.push_back((const T*)cmds[ii]->getLcbPointer());
        }
        return fn(instance, ctx, n, &l[0]);
    }
};

}

#endif
