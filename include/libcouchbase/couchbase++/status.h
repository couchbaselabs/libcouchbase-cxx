#ifndef LCB_PLUSPLUS_STATUS_H
#define LCB_PLUSPLUS_STATUS_H

namespace Couchbase {
//! @brief Status code. This wrapp an `lcb_error_t`.
//!
//! @details
//! This is returned from higher level APIs, but may wrap around any
//! lcb_error_t by simply doing
//! @code{c++}
//! Couchbase::Status s(LCB_KEY_ENOENT) ;
//! @endcode
class Status {
public:
    Status(lcb_error_t rc=LCB_SUCCESS) : code(rc) { }

    //! Check status as boolean
    //! @return true or false if operation failed or succeeded
    bool success() const { return code == LCB_SUCCESS; }

    //! If the error was caused by network issues
    //! @return true if network error
    bool isNetworkError() const { return LCB_EIFNET(code) != 0; }

    //! If error was caused by bad user input (e.g. bad parameters)
    //! @return true if input error
    bool isInputError() const { return LCB_EIFINPUT(code) != 0; }

    //! If this is a data error (missing, not found, already exists)
    //! @return true if data error
    bool isDataError() const { return LCB_EIFDATA(code) != 0; }

    //! If this is a transient error which may go away on its own
    //! @return true if transient
    bool isTemporary() const { return LCB_EIFTMP(code) != 0; }

    //! Get textual description of error (uses lcb_strerror())
    //! @return The error string. Do not free the return value
    const char *description() const { return lcb_strerror(NULL, code); }

    //! Get the raw lcb_error_t value
    //! @return the error code
    lcb_error_t errcode() const { return code; }

    operator bool() const { return success(); }
    operator lcb_error_t() const { return code; }
    operator const char * () const { return  description(); }
    Status(const Status& other) : code(other.code) {}
private:
    lcb_error_t code;
};
}

namespace std {
inline ostream& operator<< (ostream& os, const Couchbase::Status& obj) {
    os << "[0x" << std::hex << obj.errcode() << "]: " << obj.description();
    return os;
}
}

#endif
