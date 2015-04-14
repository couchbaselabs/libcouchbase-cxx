#ifndef LCB_PLUSPLUS_H
#error "include <libcouchbase/couchbase++.h>"
#endif

#include <cstdarg>
#include <cerrno>
#include <libcouchbase/couchbase.h>

namespace Couchbase {
namespace Internal {
extern "C" {
static void logcb(lcb_logprocs_st *, unsigned int, const char *, int,
    const char *, int, const char *, va_list );
}
}

class Logger : public lcb_logprocs_st {
public:
    struct Meta {
        unsigned int iid;
        int srcline;
        const char *subsys;
        const char *srcfile;
    };

    Logger(int level) : minlevel(level) {
        version = 0;
        v.v0.callback = Internal::logcb;
    }

    Logger() : Logger(LCB_LOG_WARN) {
    }

    virtual ~Logger() {
    }

    void install(Client& client) {
        lcb_cntl(client.handle(), LCB_CNTL_SET, LCB_CNTL_LOGGER,
            static_cast<lcb_logprocs_st*>(this));
    }

    virtual bool should_log(int level) {
        return level >= minlevel;
    }

    virtual void log(const Meta& message, int severity, const char *fmt, va_list ap) = 0;

protected:
    int minlevel = LCB_LOG_WARN;
};

class FileLogger : public Logger {
public:
    FileLogger(FILE *fp) : Logger(), fp(fp), should_close(false) {
    }

    FileLogger(const std::string& filename)
    : Logger(), fp(NULL), should_close(true) {
        fp = fopen(filename.c_str(), "w");
        if (!fp) {
            throw strerror(errno);
        }
    }

    virtual ~FileLogger() {
        if (should_close && fp != NULL) {
            fclose(fp);
            fp = NULL;
        }
    }

    virtual void log(const Meta& meta, int severity, const char *fmt, va_list ap) override {
        va_list apc;
        va_copy(apc, ap);

        fprintf(fp,
            "Couchbase [%d] (%s - %s:%d): ",
            meta.iid, meta.subsys, meta.srcfile, meta.srcline);
        vfprintf(fp, fmt, apc);
        fprintf(fp, "\n");
        va_end(apc);
    }

private:
    FILE *fp;
    bool should_close;
    FileLogger& operator=(FileLogger&);
    FileLogger(FileLogger&);
};

extern "C" {
static void Internal::logcb(lcb_logprocs_st *procs, unsigned int iid, const char *subsys,
    int severity, const char *srcfile, int srcline, const char *fmt, va_list ap) {

    Logger *logger = static_cast<Logger*>(procs);

    if (logger->should_log(severity)) {
        Logger::Meta m;
        m.iid = iid;
        m.srcline = srcline;
        m.subsys = subsys;
        m.srcfile = srcfile;
        logger->log(m, severity, fmt, ap);
    }
}
}

}
