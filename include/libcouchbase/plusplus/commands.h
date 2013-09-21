#ifndef LCB_CPP_COMMANDS_H
#define LCB_CPP_COMMANDS_H

#include "common.h"
#include "responses.h"
#include <cstring>
#include <string>

namespace Couchbase {

class Command {
public:
    // Returns the pointer to the underlying libcouchbase C command
    // structure
    virtual const void *getLcbPointer() const = 0;
};

class KeyCommand : public Command {
public:
    /**
     * Set the key for the operation
     * @param k the key
     * @param n the size of the key in bytes.
     *
     * Note that the key is not copied into the library and the memory
     * pointed to by 'key' must remain valid until the command is scheduled
     */
    virtual void setKey(const void *k, size_t n) = 0;
    /**
     * Set the hash key for the operation. By default this is the key itself
     * but may be modified to enforce mapping logically related keys to the
     * same server
     * @param hk the hashkey
     * @param nhk the size of the hashkey in bytes
     */
    virtual void setHashKey(const void *hk, size_t nhk) = 0;

    /**
     * Get the key for the command. This simply returns the value provided
     * for 'setKey'
     * @param n a pointer to be set to the size of the key
     * @return the pointer to the key
     */
    virtual const void *getKey(size_t *n = NULL) const = 0;

    /**
     * Get the hashkey, similar to 'getKey'
     */
    virtual const void *getHashKey(size_t *n = NULL) const = 0;

    /**
     * Convenience method to set the key
     * @param k a NUL-terminated string
     */
    void setKey(const char *k) {
        setKey(k, strlen(k));
    }

    /**
     * Convenience method to set the key
     * @param k a C++ string. Note that the string object must not be
     * modified or deleted until the operation is scheduled
     */
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

    void initFromResponse(const ResponseBase& r) {
        size_t n;
        const void *k = r.getKey(&n);
        setKey(k, n);
    }

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

    void setCas(const CasResponseBase& other) {
        setCas(other.getCas());
    }

    void initFromResponse(const CasResponseBase& r) {
        KeyCommandTmpl_v0<T>::initFromResponse(r);
        setCas(r.getCas());
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

class GetCommand : public KeyExpCommand_v0<C_GetCmd> {
public:
    GetCommand(const std::string &k) {
        KeyCommand::setKey(k);
    }

    GetCommand(){}

    void setExpiry(lcb_uint32_t e) {
        this->cmd.v.v0.exptime = e;
    }

    void setLockTime(lcb_uint32_t e) {
        setExpiry(e);
        this->cmd.v.v0.lock = 1;
    }
};

class TouchCommand : public KeyExpCommand_v0<C_TouchCmd> {
public:
    TouchCommand(const std::string &k) {
        KeyCommand::setKey(k);
    }
    TouchCommand(){}
};

class StoreCommand : public KeyCasExpCommand_v0<C_StoreCmd> {
public:

    StoreCommand(const std::string &k, const std::string &v,
                 lcb_storage_t mode = LCB_SET) {
        KeyCommand::setKey(k);
        setValue(v);
        setMode(mode);
    }

    StoreCommand(){}

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

class ArithmeticCommand : public KeyExpCommand_v0<C_ArithCmd> {
public:
    ArithmeticCommand(const std::string &k, lcb_int64_t delta) {
        KeyCommand::setKey(k);
        setDelta(delta);
    }

    ArithmeticCommand(){}

    void setDelta(lcb_int64_t delta) {
        cmd.v.v0.delta = delta;
    }
    void setInitial(lcb_uint64_t value) {
        cmd.v.v0.create = 1;
        cmd.v.v0.initial = value;
    }
};

class DeleteCommand : public KeyCasCommand_v0<C_RemoveCmd> {
public:
    DeleteCommand(const std::string &k) {
        KeyCommand::setKey(k);
    }
    DeleteCommand(){}
};

class UnlockCommand : public KeyCasCommand_v0<C_UnlockCmd> {
public:
    UnlockCommand(const std::string &k) {
        KeyCommand::setKey(k);
    }
    UnlockCommand(){}
};

class ObserveCommand : public KeyCommandTmpl_v0<C_ObserveCmd> {
};

class DurabilityCommand : public KeyCasCommand_v0<C_DurabilityCmd> {
};

} // namespace

#endif
