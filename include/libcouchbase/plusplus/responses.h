#ifndef LCB_CPP_RESPONSE_H
#define LCB_CPP_RESPONSE_H

#include "common.h"
#include <string>

namespace Couchbase {
using std::string;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/// Response Base Classes                                                    ///
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////


/**
 * This is the base response class. It is passed as a pointer to the callbacks
 * in the ResponseHandler class. This object should not be copied around
 * and its referrent is valid only throughout the scope of the callback.
 */
class ResponseBase {
public:
    /**
     * Get the key for the item.
     * @param n a pointer that will set to the size of the key, in bytes
     * @return a pointer to the key
     */
    virtual const void *getKey(lcb_size_t *n) const = 0;

    /**
     * Convenience method to get the key for the item as a C++ string
     */
    string getKey() {
        lcb_size_t n;
        const char *tmp = (const char *)getKey(&n);

        if (!n) {
            return "";
        }

        return string(tmp, n);
    }

    virtual ~ResponseBase() { }
};


// This is the base class for all responses containing a CAS. While the actual
// response class is derived from a template, the template itself always derives
// from this :)
class CasResponseBase : public ResponseBase {
public:
    virtual lcb_cas_t getCas() const = 0;
};

template <typename T, class I>
class Response : public I {
public:
    typedef T LcbInternalResponse;

    Response(const T* uresp) : resp(uresp)  {}

    virtual const void * getKey(lcb_size_t *n) const {
        *n = resp->v.v0.nkey;
        return resp->v.v0.key;
    }

    std::string getKey() const {
        size_t n;
        const void *p = getKey(&n);
        if (!n) {
            return "";
        }
        return std::string((const char *)p, n);
    }

    const T* getRawResponse() const {
        return resp;
    }

    virtual ~Response() {}

    /**
     * Converts the internal pointer to the libcouchbase response to one
     * provided inside the stack. Note that this only ensures the structure
     * itself does not go out of scope. It may be required to also copy the
     * contents (i.e. any references to pointers inside the structure) in
     * order that it may be used outside the callback
     *
     * @param newresp a pointer to the new response structure.
     */
    void transplantInner(const T *newresp) {
        resp = newresp;
    }

protected:
    const T * resp;

private:
    Response(const Response&);
};

template <typename T>
class CasResponse : public Response<T, CasResponseBase>
{
public:
    CasResponse(const T * uresp) : Response<T, CasResponseBase>(uresp) {}

    virtual lcb_cas_t getCas() const {
        return Response<T,CasResponseBase>::getRawResponse()->v.v0.cas;
    }

    virtual ~CasResponse() {}
};


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/// Operation Results                                                        ///
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
class GetResponse : public CasResponse<C_GetResp>
{

public:
    friend class SyncGetResponse;
    GetResponse(const C_GetResp *p) : CasResponse(p) {
    }

    lcb_uint32_t getFlags() const {
        return getRawResponse()->v.v0.flags;
    }

    const void *getValue(lcb_size_t *n) const {
        *n = getRawResponse()->v.v0.nbytes;
        return getRawResponse()->v.v0.bytes;
    }

    string getValue() const {
        if (getRawResponse()->v.v0.nbytes) {
            lcb_size_t n;
            const char *r = (const char *)getValue(&n);
            return string(r, n);
        }

        return "";
    }
};

class TouchResponse : public CasResponse<C_TouchResp> {
public:
    TouchResponse(const C_TouchResp *p):
        CasResponse<C_TouchResp>(p) { }

};

class StoreResponse : public CasResponse<C_StoreResp> {
public:
    StoreResponse(const C_StoreResp *p):
        CasResponse<C_StoreResp>(p) { }
};

class DeleteResponse : public CasResponse<C_RemoveResp> {
public:
    DeleteResponse(const C_RemoveResp *p):
        CasResponse<C_RemoveResp>(p) { }
};

class UnlockResponse : public Response<C_UnlockResp, ResponseBase> {
public:
    UnlockResponse(const C_UnlockResp *p):
        Response<C_UnlockResp,ResponseBase>(p) {}
};

class ArithmeticResponse : public CasResponse<C_ArithResp> {
public:
    ArithmeticResponse(const C_ArithResp *p) :
        CasResponse<C_ArithResp>(p) { }
    lcb_uint64_t getValue() const { return getRawResponse()->v.v0.value; }
};


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/// Response Handlers                                                        ///
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

class ResponseHandler;

/**
 * This structure may be subclassed by the user to contain other data related
 * to the command. It is passed as the first argument to each of the callbacks.
 */
class OperationContext {
public:
    ResponseHandler *handler;
    Connection *conn;
    virtual ~OperationContext(){}
};

// You should subclass this and define your own callbacks..
class ResponseHandler {
public:
    // This is the handler to be called as the 'default' handler. This is
    // typically useful when casting to another type, when the actual handler
    // remains further up the stack
    virtual void onDefault(OperationContext *, const ResponseBase *, lcb_error_t) {
    }

    virtual void onGet(OperationContext *c, const GetResponse* r, lcb_error_t e) {
        onDefault(c, r, e);
    }

    virtual void onStore(OperationContext *c, const StoreResponse *r, lcb_error_t e) {
        onDefault(c, r, e);
    }

    virtual void onTouch(OperationContext *c, const TouchResponse *r, lcb_error_t e) {
        onDefault(c, r, e);
    }

    virtual void onDelete(OperationContext *c, const DeleteResponse *r, lcb_error_t e) {
        onDefault(c, r, e);
    }

    virtual void onUnlock(OperationContext *c, const UnlockResponse *r, lcb_error_t e) {
        onDefault(c, r, e);
    }

    virtual void onArithmetic(OperationContext *c, const ArithmeticResponse *r, lcb_error_t e) {
        onDefault(c, r, e);
    }

    virtual ~ResponseHandler() { }
};

}

#endif
