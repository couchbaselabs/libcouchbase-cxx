#ifndef LCB_PLUSPLUS_ROWCOMMON_H
#define LCB_PLUSPLUS_ROWCOMMON_H

#include <cstdlib>
#include <cstddef>

namespace Couchbase {
namespace Internal {

template <typename TRow> class RowIterator;

template <typename TRow>
class RowProvider {
public:
    typedef RowIterator<TRow> TIter;
    virtual ~RowProvider() {}
    TIter rp_begin() { return TIter(this).mkbegin(); }
    TIter rp_end() { return TIter(this).mkend(); }

protected:
    virtual bool rp_active() const = 0;
    virtual void rp_wait() = 0;

    void rp_add(TRow&& row) {
        rows.push_back(row);
    }

private:
    std::deque<TRow> rows;
    friend RowIterator<TRow>;

    const TRow* rp_next() {
        GT_RETRY:
        if (!rows.empty()) {
            return &rows.front();
        }
        if (!rp_active()) {
            return NULL;
        }
        rp_wait();
        goto GT_RETRY;
    }
};

template <typename TRow>
class RowIterator {
public:
    const TRow& operator*() const {
        return *pp;
    }

    const TRow* operator->() const {
        return pp;
    }

    RowIterator(const RowIterator& other) : pp(other.pp), rp(other.rp) {
    }

    RowIterator& operator=(const RowIterator& other) {
        rp = other.rp;
        pp = other.pp;
        return *this;
    }

    RowIterator& operator++() {
        rp->rows.pop_front();
        pp = rp->rp_next();
        return *this;
    }

    RowIterator& operator++(int) {
        return operator ++();
    }

    bool operator!=(const RowIterator& other) const {
        return pp != other.pp;
    }

    bool operator==(const RowIterator& other) const {
        return pp == other.pp && rp == other.rp;
    }

private:
    typedef RowProvider<TRow> Provider;
    const TRow *pp = NULL;
    friend Provider;

    RowProvider<TRow> *rp = NULL;
    RowIterator(Provider *rp) : rp(rp) {}
    RowIterator& mkend() { pp = NULL; return *this; }
    RowIterator& mkbegin() { pp = rp->rp_next(); return *this; }
};

}
}

#endif
