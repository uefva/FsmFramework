//
// Lightweight thread-safe logger for the FSM framework.
//

#include "../inc/Logger.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <thread>

namespace
{
std::tm LocalTime(std::time_t timestamp)
{
    std::tm localTime = {};

#if defined(_MSC_VER)
    localtime_s(&localTime, &timestamp);
#elif defined(_WIN32)
    std::tm* timeInfo = std::localtime(&timestamp);
    if (nullptr != timeInfo)
    {
        localTime = *timeInfo;
    }
#else
    localtime_r(&timestamp, &localTime);
#endif

    return localTime;
}

const char* BaseFileName(const char* file)
{
    if (nullptr == file)
    {
        return "";
    }

    const char* baseName = file;
    for (const char* cursor = file; '\0' != *cursor; ++cursor)
    {
        if (('/' == *cursor) || ('\\' == *cursor))
        {
            baseName = cursor + 1;
        }
    }

    return baseName;
}
}

Logger::Logger() : _level(LogLevel::INFO)
{
}

Logger& Logger::Instance()
{
    static Logger logger;
    return logger;
}

void Logger::SetLevel(LogLevel level)
{
    std::lock_guard<std::mutex> guard(this->_lock);
    this->_level = level;
}

LogLevel Logger::GetLevel()
{
    std::lock_guard<std::mutex> guard(this->_lock);
    return this->_level;
}

void Logger::Log(LogLevel level,
                 const char* module,
                 const char* file,
                 int line,
                 const std::string& message)
{
    std::lock_guard<std::mutex> guard(this->_lock);

    if (!IsEnabled(level))
    {
        return;
    }

    std::cout << "[" << BuildTimestamp() << "]"
              << "[" << LogLevelToString(level) << "]"
              << "[" << module << "] "
              << message;

    if (LogLevel::INFO != level)
    {
        std::cout << " at=" << BaseFileName(file) << ":" << line
                  << " thread=" << std::this_thread::get_id();
    }

    std::cout << std::endl;
}

bool Logger::IsEnabled(LogLevel level) const
{
    return (LogLevel::OFF != this->_level) &&
           (static_cast<int>(level) >= static_cast<int>(this->_level));
}

std::string Logger::BuildTimestamp() const
{
    const auto now = std::chrono::system_clock::now();
    const auto millis =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
    const std::time_t seconds = std::chrono::system_clock::to_time_t(now);
    const std::tm localTime = LocalTime(seconds);

    std::ostringstream oss;
    oss << std::put_time(&localTime, "%H:%M:%S")
        << "." << std::setfill('0') << std::setw(3) << millis.count();
    return oss.str();
}

const char* LogLevelToString(LogLevel level)
{
    switch (level)
    {
    case LogLevel::DEBUG:
        return "DEBUG";
    case LogLevel::INFO:
        return "INFO";
    case LogLevel::WARN:
        return "WARN";
    case LogLevel::ERROR:
        return "ERROR";
    case LogLevel::OFF:
        return "OFF";
    default:
        return "UNKNOWN";
    }
}

const char* MsgTypeToString(MsgType type)
{
    switch (type)
    {
    case MSG_INIT:
        return "MSG_INIT";
    case MSG_CONNECT:
        return "MSG_CONNECT";
    case MSG_REQ:
        return "MSG_REQ";
    case MSG_RESP:
        return "MSG_RESP";
    case MSG_TIMEOUT:
        return "MSG_TIMEOUT";
    case MSG_CLOSE:
        return "MSG_CLOSE";
    default:
        return "UNKNOWN_MSG";
    }
}

const char* StateToString(Tstate state)
{
    switch (state)
    {
    case IDLE:
        return "IDLE";
    case WORKING:
        return "WORKING";
    case KILL_FSM:
        return "KILL_FSM";
    default:
        return "UNKNOWN_STATE";
    }
}

const char* ErrNoToString(EerrNo err)
{
    switch (err)
    {
    case INIT:
        return "INIT";
    case SUCCESS:
        return "SUCCESS";
    case ERROR:
        return "ERROR";
    case INVALID_STATE:
        return "INVALID_STATE";
    case INVALID_MSG:
        return "INVALID_MSG";
    case TIMER_ERROR:
        return "TIMER_ERROR";
    default:
        return "UNKNOWN_ERROR";
    }
}
