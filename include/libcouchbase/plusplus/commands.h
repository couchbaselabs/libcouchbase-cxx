#ifndef LCB_CPP_COMMANDS_H
#define LCB_CPP_COMMANDS_H

#include "common.h"
#include <cstring>
#include <string>

namespace Couchbase {

class Command {
public:
    virtual const void *getLcbPointer() const = 0;
};

class KeyCommand : public Command {
public:
    virtual void setKey(const void *k, size_t n) = 0;
    virtual void setHashKey(const void *hk, size_t nhk) = 0;
    virtual const void *getKey(size_t *n = NULL) const = 0;
    virtual const void *getHashKey(size_t *n = NULL) const = 0;

    void setKey(const char *k) {
        setKey(k, strlen(k));
    }

    void setKey(const std::string &k) {
        setKey((const void *)k.c_str(), (size_t)k.size());
    }

    void setHashKey(const char *hk) {
        setHashKey(hk, strlen(hk));
    }

    void setHashKey(const std::string &hk) {
        setHashKey(hk.c_str(), (size_t)hk.size());
    }
};

template <typename T> class KeyCommandTmpl_v0 : public KeyCommand {
public:
    using KeyCommand::setKey;
    using KeyCommand::setHashKey;

    void setKey(const void *k, size_t n) {
        cmd.v.v0.key = k;
        cmd.v.v0.nkey = n;
    }

    void setHashKey(const void *hk, size_t nhk) {
        cmd.v.v0.hashkey = hk;
        cmd.v.v0.nhashkey = nhk;
    }

    const void *getKey(size_t *n) const {
        if (n) {
            *n = cmd.v.v0.nkey;
        }
        return cmd.v.v0.key;
    }

    const void *getHashKey(size_t *n) const {
        if (n) {
            *n = cmd.v.v0.nhashkey;
        }
        return cmd.v.v0.key;
    }

    const void *getLcbPointer() const { return &cmd; }
protected:
    T cmd;
};

template <typename T> class KeyExpCommand_v0 : public KeyCommandTmpl_v0<T> {
public:
    void setExpiry(lcb_uint32_t e) {
        this->cmd.v.v0.exptime = e;
    }
};

template <typename T> class KeyCasCommand_v0 : public KeyCommandTmpl_v0<T> {
public:
    void setCas(lcb_cas_t cas) {
        this->cmd.v.v0.cas = cas;
    }
};

template <typename T> class KeyCasExpCommand_v0 : public KeyCommandTmpl_v0<T> {
public:
    void setCas(lcb_cas_t cas) {
        this->cmd.v.v0.cas = cas;
    }
    void setExpiry(lcb_uint32_t e) {
        this->cmd.v.v0.exptime = e;
    }
};

class GetCommand : public KeyExpCommand_v0<LcbGetCommand> {
public:
    void setExpiry(lcb_uint32_t e) {
        this->cmd.v.v0.exptime = e;
    }

    void setLockTime(lcb_uint32_t e) {
        setExpiry(e);
        this->cmd.v.v0.lock = 1;
    }
};

class TouchCommand : public KeyExpCommand_v0<LcbTouchCommand> {};

class StoreCommand : public KeyCasExpCommand_v0<LcbStoreCommand> {
public:
    void setMode(lcb_storage_t m) {
        cmd.v.v0.operation = m;
    }

    void setValue(const void *v, size_t n) {
        cmd.v.v0.bytes = v;
        cmd.v.v0.nbytes = n;
    }

    void setValue(const char *v) {
        setValue(v, strlen(v));
    }

    void setValue(const std::string &v) {
        setValue(v.c_str(), (size_t)v.size());
    }

    void setFlags(lcb_uint32_t f) {
        cmd.v.v0.flags = f;
    }

    void setDataType(lcb_uint8_t d) {
        cmd.v.v0.datatype = d;
    }
};

class ArithmeticCommand : public KeyExpCommand_v0<LcbArithmeticCommand> {
public:
    void setDelta(lcb_int64_t delta) {
        cmd.v.v0.delta = delta;
    }
    void setInitial(lcb_uint64_t value) {
        cmd.v.v0.create = 1;
        cmd.v.v0.initial = value;
    }
};

class DeleteCommand : public KeyCasCommand_v0<LcbDeleteCommand> {
};

class UnlockCommand : public KeyCasCommand_v0<LcbUnlockCommand> {
};

class ObserveCommand : public KeyCommandTmpl_v0<LcbObserveCommand> {
};

class DurabilityCommand : public KeyCasCommand_v0<LcbDurabilityCommand> {
};

} // namespace

#endif
