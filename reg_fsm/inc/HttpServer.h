//
// HTTP server adapter for the FSM framework.
//

#ifndef MYFSMDEMO_HTTPSERVER_H
#define MYFSMDEMO_HTTPSERVER_H

#include <memory>
#include <string>

#include "common.h"

class Cfactory_mgr;

struct HttpServerConfig
{
    HttpServerConfig()
        : host("0.0.0.0"), port(8080), syncTimeoutMs(5000)
    {
    }

    std::string host;
    unsigned short port;
    unsigned int syncTimeoutMs;
};

class HttpServer
{
public:
    HttpServer(Cfactory_mgr& manager, const HttpServerConfig& config);
    ~HttpServer();

    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;

    EerrNo Start();
    void Stop();
    bool IsRunning() const;
    unsigned short GetPort() const;

private:
    class Impl;
    std::unique_ptr<Impl> _impl;
};

#endif // MYFSMDEMO_HTTPSERVER_H
