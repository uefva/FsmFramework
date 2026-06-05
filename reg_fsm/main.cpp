//
// HTTP service entry point for the FSM framework.
//

#include <csignal>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <thread>

#include "inc/AuthFactory.h"
#include "inc/Cfactory_mgr.h"
#include "inc/HttpServer.h"
#include "inc/Logger.h"
#include "inc/RegFactory.h"

namespace
{
volatile std::sig_atomic_t g_stopRequested = 0;

struct ServiceOptions
{
    HttpServerConfig http;
    LogLevel logLevel;

    ServiceOptions() : logLevel(LogLevel::INFO)
    {
    }
};

void HandleSignal(int signal)
{
    (void)signal;
    g_stopRequested = 1;
}

bool StartsWith(const std::string& value, const std::string& prefix)
{
    return 0 == value.compare(0, prefix.size(), prefix);
}

bool ParseUnsignedInt(const std::string& text, unsigned int& value)
{
    if (text.empty())
    {
        return false;
    }

    char* end = nullptr;
    const unsigned long parsed = std::strtoul(text.c_str(), &end, 10);
    if ((nullptr == end) || ('\0' != *end) ||
        (parsed > std::numeric_limits<unsigned int>::max()))
    {
        return false;
    }

    value = static_cast<unsigned int>(parsed);
    return true;
}

bool ParseLogLevel(const std::string& text, LogLevel& level)
{
    if ("debug" == text)
    {
        level = LogLevel::DEBUG;
    }
    else if ("info" == text)
    {
        level = LogLevel::INFO;
    }
    else if ("warn" == text)
    {
        level = LogLevel::WARN;
    }
    else if ("error" == text)
    {
        level = LogLevel::ERROR;
    }
    else if ("off" == text)
    {
        level = LogLevel::OFF;
    }
    else
    {
        return false;
    }

    return true;
}

void PrintUsage(const char* program)
{
    std::cout << "Usage: " << program
              << " [--host=0.0.0.0]"
              << " [--port=8080]"
              << " [--log-level=debug|info|warn|error|off]"
              << " [--sync-timeout-ms=5000]"
              << std::endl;
}

bool ParseArgs(int argc, char* argv[], ServiceOptions& options)
{
    for (int index = 1; index < argc; ++index)
    {
        const std::string arg(argv[index]);
        if (("--help" == arg) || ("-h" == arg))
        {
            PrintUsage(argv[0]);
            return false;
        }
        else if (StartsWith(arg, "--host="))
        {
            options.http.host = arg.substr(7);
        }
        else if (StartsWith(arg, "--port="))
        {
            unsigned int port = 0;
            if (!ParseUnsignedInt(arg.substr(7), port) ||
                (0 == port) ||
                (port > 65535))
            {
                return false;
            }
            options.http.port = static_cast<unsigned short>(port);
        }
        else if (StartsWith(arg, "--log-level="))
        {
            if (!ParseLogLevel(arg.substr(12), options.logLevel))
            {
                return false;
            }
        }
        else if (StartsWith(arg, "--sync-timeout-ms="))
        {
            unsigned int timeoutMs = 0;
            if (!ParseUnsignedInt(arg.substr(18), timeoutMs) || (0 == timeoutMs))
            {
                return false;
            }
            options.http.syncTimeoutMs = timeoutMs;
        }
        else
        {
            return false;
        }
    }

    return true;
}
}

int main(int argc, char* argv[])
{
    ServiceOptions options;
    if (!ParseArgs(argc, argv, options))
    {
        PrintUsage(argv[0]);
        return 1;
    }

    Logger::Instance().SetLevel(options.logLevel);

    std::signal(SIGINT, HandleSignal);
    std::signal(SIGTERM, HandleSignal);

    Cfactory_mgr manager;
    if (SUCCESS != manager.RegisterFactory(new RegFactory(FAC_REG_FAC_ID)))
    {
        LOG_ERROR("main", "failed to register RegFactory");
        return 2;
    }

    if (SUCCESS != manager.RegisterFactory(new AuthFactory(FAC_AUTH_FAC_ID)))
    {
        LOG_ERROR("main", "failed to register AuthFactory");
        return 2;
    }

    if (SUCCESS != manager.Start())
    {
        LOG_ERROR("main", "failed to start FSM manager");
        return 2;
    }

    HttpServer httpServer(manager, options.http);
    if (SUCCESS != httpServer.Start())
    {
        manager.Stop();
        LOG_ERROR("main", "failed to start HTTP server");
        return 2;
    }

    LOG_INFO("main", "service started, press Ctrl+C to stop");

    while (0 == g_stopRequested)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    LOG_INFO("main", "stopping service");
    httpServer.Stop();
    manager.Stop();

    return 0;
}
