#ifndef LCB_CPP_CONNECTION_H
#define LCB_CPP_CONNECTION_H

#include "common.h"
#include "commands.h"
#include "responses.h"
#include <vector>

namespace Couchbase {

namespace Internal {
    template <typename T> class CommandList {
    public:
        std::vector<const T*> impl;

        template <typename C>
        CommandList(C **cmds, size_t ncmds) {
            impl.reserve(ncmds);
            for (size_t ii = 0; ii < ncmds; ii++) {
                impl.push_back((T*)cmds[ii]->getLcbPointer());
            }
        }

        const T * const * getList() const {
            return &impl[0];
        }
    };
}

class OperationContext;
class Connection
{

public:
    Connection(lcb_error_t &status,
               const std::string &host = "localhost",
               const std::string &bkt="default",
               const std::string &user = "",
               const std::string &pass = "",
               lcb_io_opt_t io = NULL);

    Connection(lcb_t existing, ResponseHandler *resphandler): instance(existing) {
    }

    ~Connection() {
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

    // Operations
    lcb_error_t scheduleGet(GetCommand **commands, size_t ncmds, OperationContext *ctx) {
        Internal::CommandList<lcb_get_cmd_t> l(commands, ncmds);
        return lcb_get(instance, ctx, ncmds, l.getList());
    }

    lcb_error_t scheduleGet(GetCommand *cmd, OperationContext *ctx) {
        return scheduleGet(&cmd, 1, ctx);
    }

    lcb_error_t scheduleStore(StoreCommand **commands, size_t ncmds,
                              OperationContext *ctx) {
        Internal::CommandList<lcb_store_cmd_t> l(commands, ncmds);
        return lcb_store(instance, ctx, ncmds, l.getList());
    }

    lcb_error_t scheduleStore(StoreCommand *cmd, OperationContext *ctx) {
        return scheduleStore(&cmd, 1, ctx);
    }

    lcb_error_t scheduleDelete(DeleteCommand **commands, size_t ncmds,
                               OperationContext *ctx) {
        Internal::CommandList<lcb_remove_cmd_t> l(commands, ncmds);
        return lcb_remove(instance, ctx, ncmds, l.getList());
    }

    lcb_error_t scheduleDelete(DeleteCommand *cmd, OperationContext *ctx) {
        return scheduleDelete(&cmd, 1, ctx);
    }

    lcb_error_t scheduleArithmetic(ArithmeticCommand **commands, size_t ncmds,
                                   OperationContext *ctx) {
        Internal::CommandList<lcb_arithmetic_cmd_t> l(commands, ncmds);
        return lcb_arithmetic(instance, ctx, ncmds, l.getList());
    }
    lcb_error_t scheduleArithmetic(ArithmeticCommand *cmd, OperationContext *ctx) {
        return scheduleArithmetic(&cmd, 1, ctx);
    }

    lcb_error_t scheduleUnlock(UnlockCommand **commands, size_t ncmds,
                               OperationContext *ctx) {
        Internal::CommandList<lcb_unlock_cmd_t> l(commands, ncmds);
        return lcb_unlock(instance, ctx, ncmds, l.getList());
    }

    lcb_error_t scheduleTouch(TouchCommand **commands, size_t ncmds,
                              OperationContext *ctx) {
        Internal::CommandList<lcb_touch_cmd_t> l(commands, ncmds);
        return lcb_touch(instance, ctx, ncmds, l.getList());
    }

protected:
    struct lcb_create_st creationOptions;
    lcb_t instance;
    std::string hostname;
    std::string bucket;
    std::string username;
    std::string password;

private:
    Connection(Connection&);
    void initCallbacks();
};

}

#endif
