//
// HTTP server adapter for the FSM framework.
//

#include "../inc/HttpServer.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <thread>
#include <utility>
#include <vector>

#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/json.hpp>

#include "../inc/Cfactory_mgr.h"
#include "../inc/Logger.h"

namespace
{
namespace net = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;
using tcp = net::ip::tcp;

const char* HTTP_MODULE = "HttpServer";

struct WaitKey
{
    unsigned int serviceId;
    unsigned int sessionId;

    bool operator<(const WaitKey& other) const
    {
        if (serviceId != other.serviceId)
        {
            return serviceId < other.serviceId;
        }

        return sessionId < other.sessionId;
    }
};

struct WaitEntry
{
    WaitEntry() : completed(false)
    {
    }

    std::mutex lock;
    std::condition_variable cv;
    bool completed;
    FsmCompletionEvent event;
};

std::string ToString(beast::string_view text)
{
    return std::string(text.data(), text.size());
}

std::string ExtractPath(const std::string& target)
{
    const std::string::size_type queryPos = target.find('?');
    return (std::string::npos == queryPos)
        ? target
        : target.substr(0, queryPos);
}

std::string ExtractQuery(const std::string& target)
{
    const std::string::size_type queryPos = target.find('?');
    return (std::string::npos == queryPos)
        ? std::string()
        : target.substr(queryPos + 1);
}

bool QueryHasWaitTrue(const std::string& query)
{
    std::string::size_type begin = 0;
    while (begin <= query.size())
    {
        const std::string::size_type end = query.find('&', begin);
        const std::string item = (std::string::npos == end)
            ? query.substr(begin)
            : query.substr(begin, end - begin);

        if (("wait=true" == item) || ("wait=1" == item))
        {
            return true;
        }

        if (std::string::npos == end)
        {
            break;
        }
        begin = end + 1;
    }

    return false;
}

bool ResolveRoute(const std::string& path,
                  unsigned int& serviceId,
                  const char*& serviceName)
{
    if ("/api/v1/register" == path)
    {
        serviceId = FAC_REG_FAC_ID;
        serviceName = "register";
        return true;
    }

    if ("/api/v1/auth" == path)
    {
        serviceId = FAC_AUTH_FAC_ID;
        serviceName = "auth";
        return true;
    }

    return false;
}

http::response<http::string_body> JsonResponse(
    http::status status,
    unsigned int version,
    const json::object& body)
{
    http::response<http::string_body> response(status, version);
    response.set(http::field::server, "FsmFramework");
    response.set(http::field::content_type, "application/json");
    response.keep_alive(false);
    response.body() = json::serialize(body);
    response.prepare_payload();
    return response;
}

http::response<http::string_body> ErrorResponse(
    http::status status,
    unsigned int version,
    const char* error,
    const std::string& message)
{
    json::object body;
    body["status"] = "error";
    body["error"] = error;
    body["message"] = message;
    return JsonResponse(status, version, body);
}

bool ReadSessionId(const json::object& body,
                   unsigned int& sessionId,
                   std::string& error)
{
    const json::object::const_iterator it = body.find("sessionId");
    if (body.end() == it)
    {
        return true;
    }

    std::uint64_t rawSessionId = 0;
    const json::value& value = it->value();
    if (value.is_uint64())
    {
        rawSessionId = value.as_uint64();
    }
    else if (value.is_int64() && (value.as_int64() > 0))
    {
        rawSessionId = static_cast<std::uint64_t>(value.as_int64());
    }
    else
    {
        error = "sessionId must be a positive integer";
        return false;
    }

    if ((0 == rawSessionId) ||
        (rawSessionId > std::numeric_limits<unsigned int>::max()))
    {
        error = "sessionId is out of range";
        return false;
    }

    sessionId = static_cast<unsigned int>(rawSessionId);
    return true;
}
}

class HttpServer::Impl
{
public:
    Impl(Cfactory_mgr& manager, const HttpServerConfig& config)
        : _manager(manager),
          _config(config),
          _ioc(),
          _acceptor(_ioc),
          _running(false),
          _nextSessionId(1),
          _actualPort(0)
    {
    }

    ~Impl()
    {
        Stop();
    }

    EerrNo Start()
    {
        bool expected = false;
        if (!_running.compare_exchange_strong(expected, true))
        {
            return ERROR;
        }

        _ioc.restart();

        boost::system::error_code ec;
        const net::ip::address address = net::ip::make_address(_config.host, ec);
        if (ec)
        {
            _running.store(false);
            LOG_ERROR(HTTP_MODULE, "invalid host=" << _config.host);
            return ERROR;
        }

        const tcp::endpoint endpoint(address, _config.port);
        _acceptor.open(endpoint.protocol(), ec);
        if (ec)
        {
            _running.store(false);
            LOG_ERROR(HTTP_MODULE, "open failed, error=" << ec.message());
            return ERROR;
        }

        _acceptor.set_option(net::socket_base::reuse_address(true), ec);
        if (ec)
        {
            Stop();
            LOG_ERROR(HTTP_MODULE, "set_option failed, error=" << ec.message());
            return ERROR;
        }

        _acceptor.bind(endpoint, ec);
        if (ec)
        {
            Stop();
            LOG_ERROR(HTTP_MODULE, "bind failed, host=" << _config.host
                                                        << " port=" << _config.port
                                                        << " error=" << ec.message());
            return ERROR;
        }

        _acceptor.listen(net::socket_base::max_listen_connections, ec);
        if (ec)
        {
            Stop();
            LOG_ERROR(HTTP_MODULE, "listen failed, error=" << ec.message());
            return ERROR;
        }

        _actualPort.store(_acceptor.local_endpoint().port());
        _manager.SetCompletionCallback(
            [this](const FsmCompletionEvent& event) { this->OnCompletion(event); });

        DoAccept();

        const unsigned int threadCount = 2;
        for (unsigned int index = 0; index < threadCount; ++index)
        {
            _threads.push_back(std::thread([this]() { this->_ioc.run(); }));
        }

        LOG_INFO(HTTP_MODULE, "listening host=" << _config.host
                             << " port=" << _actualPort.load());

        return SUCCESS;
    }

    void Stop()
    {
        const bool wasRunning = _running.exchange(false);
        if (!wasRunning)
        {
            return;
        }

        _manager.SetCompletionCallback(Cfactory_mgr::CompletionCallback());

        boost::system::error_code ec;
        _acceptor.close(ec);
        NotifyAllWaitersAsStopped();

        _ioc.stop();
        for (std::thread& thread : _threads)
        {
            if (thread.joinable())
            {
                thread.join();
            }
        }
        _threads.clear();

        _actualPort.store(0);

        LOG_INFO(HTTP_MODULE, "stopped");
    }

    bool IsRunning() const
    {
        return _running.load();
    }

    unsigned short GetPort() const
    {
        return static_cast<unsigned short>(_actualPort.load());
    }

    http::response<http::string_body> HandleRequest(
        const http::request<http::string_body>& request)
    {
        const std::string target = ToString(request.target());
        const std::string path = ExtractPath(target);
        const std::string query = ExtractQuery(target);

        unsigned int serviceId = 0;
        const char* serviceName = "";
        if (!ResolveRoute(path, serviceId, serviceName))
        {
            return ErrorResponse(http::status::not_found,
                                 request.version(),
                                 "not_found",
                                 "unknown endpoint");
        }

        if (http::verb::post != request.method())
        {
            return ErrorResponse(http::status::method_not_allowed,
                                 request.version(),
                                 "method_not_allowed",
                                 "only POST is supported");
        }

        boost::system::error_code parseError;
        json::value payloadValue = json::object();
        if (!request.body().empty())
        {
            payloadValue = json::parse(request.body(), parseError);
            if (parseError || !payloadValue.is_object())
            {
                return ErrorResponse(http::status::bad_request,
                                     request.version(),
                                     "invalid_json",
                                     "request body must be a JSON object");
            }
        }

        unsigned int sessionId = GenerateSessionId();
        std::string sessionError;
        if (!ReadSessionId(payloadValue.as_object(), sessionId, sessionError))
        {
            return ErrorResponse(http::status::bad_request,
                                 request.version(),
                                 "invalid_session_id",
                                 sessionError);
        }

        CMsg msg;
        msg.type = MSG_INIT;
        msg.serviceId = serviceId;
        msg.fsmId = 0;
        msg.sessionId = sessionId;

        if (!request.body().empty())
        {
            const std::string serializedPayload = json::serialize(payloadValue);
            msg.msg.assign(serializedPayload.begin(), serializedPayload.end());
        }

        if (QueryHasWaitTrue(query))
        {
            return HandleSyncRequest(request, msg, serviceName);
        }

        const EerrNo sendResult = _manager.SendMsg(msg);
        if (SUCCESS != sendResult)
        {
            return ErrorResponse(http::status::service_unavailable,
                                 request.version(),
                                 "service_unavailable",
                                 ErrNoToString(sendResult));
        }

        json::object body;
        body["sessionId"] = static_cast<std::uint64_t>(sessionId);
        body["service"] = serviceName;
        body["status"] = "accepted";
        return JsonResponse(http::status::accepted, request.version(), body);
    }

private:
    class Session : public std::enable_shared_from_this<Session>
    {
    public:
        Session(tcp::socket socket, Impl& server)
            : _stream(std::move(socket)), _server(server)
        {
        }

        void Run()
        {
            DoRead();
        }

    private:
        void DoRead()
        {
            _request = http::request<http::string_body>();
            _stream.expires_after(std::chrono::seconds(30));

            std::shared_ptr<Session> self = shared_from_this();
            http::async_read(
                _stream,
                _buffer,
                _request,
                [self](
                    boost::system::error_code ec,
                    std::size_t bytesTransferred) {
                    self->OnRead(ec, bytesTransferred);
                });
        }

        void OnRead(boost::system::error_code ec, std::size_t bytesTransferred)
        {
            (void)bytesTransferred;
            if (http::error::end_of_stream == ec)
            {
                Close();
                return;
            }

            if (ec)
            {
                LOG_WARN(HTTP_MODULE, "read failed, error=" << ec.message());
                return;
            }

            std::shared_ptr<http::response<http::string_body>> response(
                new http::response<http::string_body>(_server.HandleRequest(_request)));

            std::shared_ptr<Session> self = shared_from_this();
            http::async_write(
                _stream,
                *response,
                [self, response](
                    boost::system::error_code writeEc,
                    std::size_t writeBytes) {
                    self->OnWrite(writeEc, writeBytes);
                });
        }

        void OnWrite(boost::system::error_code ec, std::size_t bytesTransferred)
        {
            (void)bytesTransferred;
            if (ec)
            {
                LOG_WARN(HTTP_MODULE, "write failed, error=" << ec.message());
            }

            Close();
        }

        void Close()
        {
            boost::system::error_code ec;
            beast::get_lowest_layer(_stream).socket().shutdown(tcp::socket::shutdown_send, ec);
        }

    private:
        beast::tcp_stream _stream;
        beast::flat_buffer _buffer;
        http::request<http::string_body> _request;
        Impl& _server;
    };

    void DoAccept()
    {
        if (!_running.load())
        {
            return;
        }

        _acceptor.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket) {
                if (!_running.load())
                {
                    return;
                }

                if (ec)
                {
                    LOG_WARN(HTTP_MODULE, "accept failed, error=" << ec.message());
                }
                else
                {
                    std::shared_ptr<Session> session(new Session(std::move(socket), *this));
                    session->Run();
                }

                DoAccept();
            });
    }

    unsigned int GenerateSessionId()
    {
        unsigned int sessionId = _nextSessionId.fetch_add(1);
        while (0 == sessionId)
        {
            sessionId = _nextSessionId.fetch_add(1);
        }

        return sessionId;
    }

    std::shared_ptr<WaitEntry> RegisterWaiter(unsigned int serviceId,
                                              unsigned int sessionId)
    {
        std::shared_ptr<WaitEntry> waiter(new WaitEntry());
        const WaitKey key = {serviceId, sessionId};

        std::lock_guard<std::mutex> guard(_waitersLock);
        _waiters[key].push_back(waiter);
        return waiter;
    }

    void UnregisterWaiter(unsigned int serviceId,
                          unsigned int sessionId,
                          const std::shared_ptr<WaitEntry>& waiter)
    {
        const WaitKey key = {serviceId, sessionId};

        std::lock_guard<std::mutex> guard(_waitersLock);
        std::map<WaitKey, std::vector<std::weak_ptr<WaitEntry>>>::iterator it =
            _waiters.find(key);
        if (_waiters.end() == it)
        {
            return;
        }

        std::vector<std::weak_ptr<WaitEntry>>& entries = it->second;
        entries.erase(
            std::remove_if(
                entries.begin(),
                entries.end(),
                [&waiter](const std::weak_ptr<WaitEntry>& weakEntry) {
                    std::shared_ptr<WaitEntry> locked = weakEntry.lock();
                    return (!locked || (locked == waiter));
                }),
            entries.end());

        if (entries.empty())
        {
            _waiters.erase(it);
        }
    }

    void OnCompletion(const FsmCompletionEvent& event)
    {
        std::vector<std::shared_ptr<WaitEntry>> entriesToNotify;
        const WaitKey key = {event.serviceId, event.sessionId};

        {
            std::lock_guard<std::mutex> guard(_waitersLock);
            std::map<WaitKey, std::vector<std::weak_ptr<WaitEntry>>>::iterator it =
                _waiters.find(key);
            if (_waiters.end() != it)
            {
                for (std::weak_ptr<WaitEntry>& weakEntry : it->second)
                {
                    std::shared_ptr<WaitEntry> entry = weakEntry.lock();
                    if (entry)
                    {
                        entriesToNotify.push_back(entry);
                    }
                }
            }
        }

        for (std::shared_ptr<WaitEntry>& entry : entriesToNotify)
        {
            {
                std::lock_guard<std::mutex> entryGuard(entry->lock);
                entry->event = event;
                entry->completed = true;
            }
            entry->cv.notify_all();
        }
    }

    void NotifyAllWaitersAsStopped()
    {
        std::vector<std::shared_ptr<WaitEntry>> entriesToNotify;

        {
            std::lock_guard<std::mutex> guard(_waitersLock);
            for (std::map<WaitKey, std::vector<std::weak_ptr<WaitEntry>>>::iterator it =
                     _waiters.begin();
                 it != _waiters.end();
                 ++it)
            {
                for (std::weak_ptr<WaitEntry>& weakEntry : it->second)
                {
                    std::shared_ptr<WaitEntry> entry = weakEntry.lock();
                    if (entry)
                    {
                        entriesToNotify.push_back(entry);
                    }
                }
            }
            _waiters.clear();
        }

        for (std::shared_ptr<WaitEntry>& entry : entriesToNotify)
        {
            {
                std::lock_guard<std::mutex> entryGuard(entry->lock);
                entry->event.result = INVALID_STATE;
                entry->completed = true;
            }
            entry->cv.notify_all();
        }
    }

    http::response<http::string_body> HandleSyncRequest(
        const http::request<http::string_body>& request,
        const CMsg& msg,
        const char* serviceName)
    {
        std::shared_ptr<WaitEntry> waiter =
            RegisterWaiter(msg.serviceId, msg.sessionId);

        const EerrNo sendResult = _manager.SendMsg(msg);
        if (SUCCESS != sendResult)
        {
            UnregisterWaiter(msg.serviceId, msg.sessionId, waiter);
            return ErrorResponse(http::status::service_unavailable,
                                 request.version(),
                                 "service_unavailable",
                                 ErrNoToString(sendResult));
        }

        std::unique_lock<std::mutex> lock(waiter->lock);
        const bool completed = waiter->cv.wait_for(
            lock,
            std::chrono::milliseconds(_config.syncTimeoutMs),
            [&waiter]() { return waiter->completed; });

        FsmCompletionEvent completion = waiter->event;
        lock.unlock();

        UnregisterWaiter(msg.serviceId, msg.sessionId, waiter);

        if (!completed)
        {
            return ErrorResponse(http::status::gateway_timeout,
                                 request.version(),
                                 "timeout",
                                 "FSM flow did not complete before timeout");
        }

        if (SUCCESS != completion.result)
        {
            return ErrorResponse(http::status::internal_server_error,
                                 request.version(),
                                 "fsm_failed",
                                 ErrNoToString(completion.result));
        }

        json::object body;
        body["sessionId"] = static_cast<std::uint64_t>(completion.sessionId);
        body["service"] = serviceName;
        body["fsmId"] = static_cast<std::uint64_t>(completion.fsmId);
        body["result"] = ErrNoToString(completion.result);
        body["status"] = "completed";
        return JsonResponse(http::status::ok, request.version(), body);
    }

private:
    Cfactory_mgr& _manager;
    HttpServerConfig _config;
    net::io_context _ioc;
    tcp::acceptor _acceptor;
    std::atomic<bool> _running;
    std::atomic<unsigned int> _nextSessionId;
    std::atomic<unsigned int> _actualPort;
    std::vector<std::thread> _threads;
    std::mutex _waitersLock;
    std::map<WaitKey, std::vector<std::weak_ptr<WaitEntry>>> _waiters;
};

HttpServer::HttpServer(Cfactory_mgr& manager, const HttpServerConfig& config)
    : _impl(new Impl(manager, config))
{
}

HttpServer::~HttpServer()
{
}

EerrNo HttpServer::Start()
{
    return _impl->Start();
}

void HttpServer::Stop()
{
    _impl->Stop();
}

bool HttpServer::IsRunning() const
{
    return _impl->IsRunning();
}

unsigned short HttpServer::GetPort() const
{
    return _impl->GetPort();
}
