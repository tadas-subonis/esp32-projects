#include "artillery/log.hpp"

#include <cstdarg>
#include <cstdio>

namespace artillery {
namespace {

LogSink s_sink = nullptr;

}  // namespace

void set_log_sink(LogSink sink) { s_sink = sink; }

void log_msg(const char* fmt, ...)
{
    if (s_sink == nullptr || fmt == nullptr) {
        return;
    }
    char buf[192];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    buf[sizeof(buf) - 1] = 0;
    s_sink(buf);
}

}  // namespace artillery
