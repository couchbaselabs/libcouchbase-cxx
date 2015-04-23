// Forward-declare classes so we have less clutter in the header.
// this file is included _before_ any definitions.
namespace Couchbase {

class Client;
class Status;
class Context;
class DurabilityOptions;
class Handler;
class Status;

namespace Internal {
    template <typename T> class MultiContextT;
    template<typename T> using MultiContext = MultiContextT<T>;
    typedef MultiContext<lcb_CMDENDURE> MultiDurContext;
    typedef MultiContext<lcb_CMDOBSERVE> MultiObsContext;
}

namespace OpInfo {
struct Base {
    typedef lcb_CMDBASE CType;
    typedef lcb_RESPBASE RType;
};
struct Get {
    typedef lcb_CMDGET CType;
    typedef lcb_RESPGET RType;
};
struct Store {
    typedef lcb_CMDSTORE CType;
    typedef lcb_RESPSTORE RType;
};
struct Touch {
    typedef lcb_CMDTOUCH CType;
    typedef lcb_RESPTOUCH RType;
};
struct Remove {
    typedef lcb_CMDREMOVE CType;
    typedef lcb_RESPREMOVE RType;
};
struct Unlock {
    typedef lcb_CMDUNLOCK CType;
    typedef lcb_RESPUNLOCK RType;
};
struct Counter {
    typedef lcb_CMDCOUNTER CType;
    typedef lcb_RESPCOUNTER RType;
};
struct Stats {
    typedef lcb_CMDSTATS CType;
    typedef lcb_RESPSTATS RType;
};
struct Observe {
    typedef lcb_CMDOBSERVE CType;
    typedef lcb_RESPOBSERVE RType;
};
struct Endure {
    typedef lcb_CMDENDURE CType;
    typedef lcb_RESPENDURE RType;
};
}
}
