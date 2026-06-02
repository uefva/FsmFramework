//
// Lightweight thread-safe logger for the FSM framework.
//

#ifndef MYFSMDEMO_LOGGER_H
#define MYFSMDEMO_LOGGER_H

#include <mutex>
#include <sstream>
#include <string>

#include "common.h"

enum class LogLevel
{
    DEBUG = 0,
    INFO,
    WARN,
    ERROR,
    OFF
};

class Logger
{
public:
    static Logger& Instance();

    void SetLevel(LogLevel level);
    LogLevel GetLevel();

    void Log(LogLevel level,
             const char* module,
             const char* file,
             int line,
             const std::string& message);

private:
    Logger();

    bool IsEnabled(LogLevel level) const;
    std::string BuildTimestamp() const;

private:
    std::mutex _lock;
    LogLevel _level;
};

const char* LogLevelToString(LogLevel level);
const char* MsgTypeToString(MsgType type);
const char* StateToString(Tstate state);
const char* ErrNoToString(EerrNo err);

#define LOG_DEBUG(module, message)                                             \
    do                                                                         \
    {                                                                          \
        std::ostringstream fsm_log_oss;                                        \
        fsm_log_oss << message;                                                \
        Logger::Instance().Log(LogLevel::DEBUG, module, __FILE__, __LINE__,    \
                               fsm_log_oss.str());                             \
    } while (0)

#define LOG_INFO(module, message)                                              \
    do                                                                         \
    {                                                                          \
        std::ostringstream fsm_log_oss;                                        \
        fsm_log_oss << message;                                                \
        Logger::Instance().Log(LogLevel::INFO, module, __FILE__, __LINE__,     \
                               fsm_log_oss.str());                             \
    } while (0)

#define LOG_WARN(module, message)                                              \
    do                                                                         \
    {                                                                          \
        std::ostringstream fsm_log_oss;                                        \
        fsm_log_oss << message;                                                \
        Logger::Instance().Log(LogLevel::WARN, module, __FILE__, __LINE__,     \
                               fsm_log_oss.str());                             \
    } while (0)

#define LOG_ERROR(module, message)                                             \
    do                                                                         \
    {                                                                          \
        std::ostringstream fsm_log_oss;                                        \
        fsm_log_oss << message;                                                \
        Logger::Instance().Log(LogLevel::ERROR, module, __FILE__, __LINE__,    \
                               fsm_log_oss.str());                             \
    } while (0)

#endif // MYFSMDEMO_LOGGER_H
