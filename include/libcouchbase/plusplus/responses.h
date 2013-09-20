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


// This is the base class for all responses. While the actual response clases
// use templates, they still all inherit from this
class ResponseBase {
public:
    virtual const void *getKey(lcb_size_t *n) const = 0;
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

template <typename T, class I> class Response : public I {
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

    // Converts the libcouchbase-allocated response to one provided by the
    // C++ binding
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
class GetResponse : public CasResponse<LcbGetResponse>
{

public:
    friend class SyncGetResponse;
    GetResponse(const LcbGetResponse *p) : CasResponse(p) {
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

class TouchResponse : public CasResponse<LcbTouchResponse> {
public:
    TouchResponse(const LcbTouchResponse *p): CasResponse<LcbTouchResponse>(p) { }

};

class StoreResponse : public CasResponse<LcbStoreResponse> {
public:
    StoreResponse(const LcbStoreResponse *p): CasResponse<LcbStoreResponse>(p) { }
};

class DeleteResponse : public CasResponse<LcbDeleteResponse> {
public:
    DeleteResponse(const LcbDeleteResponse *p): CasResponse<LcbDeleteResponse>(p) { }
};

class UnlockResponse : public Response<LcbUnlockResponse, ResponseBase> {
public:
    UnlockResponse(const LcbUnlockResponse *p): Response<LcbUnlockResponse,ResponseBase>(p) {}
};

class ArithmeticResponse : public CasResponse<LcbArithmeticResponse> {
public:
    ArithmeticResponse(const LcbArithmeticResponse *p) : CasResponse<LcbArithmeticResponse>(p) { }
    lcb_uint64_t getValue() const { return getRawResponse()->v.v0.value; }
};


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/// Response Handlers                                                        ///
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

class ResponseHandler;
struct OperationContext {
    ResponseHandler *handler;
};

// You should subclass this and define your own callbacks..
class ResponseHandler {
public:
    virtual void onGet(OperationContext*, const GetResponse*, lcb_error_t) {
    }

    virtual void onStore(OperationContext*, const StoreResponse*, lcb_error_t) {
    }

    virtual void onTouch(OperationContext*, const TouchResponse*, lcb_error_t) {
    }

    virtual void onDelete(OperationContext*, const DeleteResponse*, lcb_error_t) {
    }

    virtual void onUnlock(OperationContext*, const UnlockResponse*, lcb_error_t) {
    }

    virtual void onArithmetic(OperationContext*, const ArithmeticResponse *, lcb_error_t) {
    }

    virtual ~ResponseHandler() { }
};

}

#endif
