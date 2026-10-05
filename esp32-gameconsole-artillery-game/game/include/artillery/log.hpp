#pragma once

namespace artillery {

using LogSink = void (*)(const char* line);

void set_log_sink(LogSink sink);
void log_msg(const char* fmt, ...);

}  // namespace artillery
