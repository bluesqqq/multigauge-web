#pragma once

#include <multigauge/io/Logger.h>

class LoggerWebConsole : public Logger {
    protected:
        void _log(LogLevel level, const char* tag, const char* fmt, va_list args) override;
    public:
        bool init() override;
};