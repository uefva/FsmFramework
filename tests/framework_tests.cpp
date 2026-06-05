//
// Created by MR on 2026/5/28.
//

#include <cassert>
#include <chrono>
#include <string>
#include <thread>

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/json.hpp>

#include "../reg_fsm/inc/AuthFactory.h"
#include "../reg_fsm/inc/AuthFsm.h"
#include "../reg_fsm/inc/CMsg.h"
#include "../reg_fsm/inc/Cfactory_mgr.h"
#include "../reg_fsm/inc/HttpServer.h"
#include "../reg_fsm/inc/Logger.h"
#include "../reg_fsm/inc/RegFactory.h"
#include "../reg_fsm/inc/RegFsm.h"

namespace
{
namespace net = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;
using tcp = net::ip::tcp;

http::response<http::string_body> SendHttpRequest(
    unsigned short port,
    http::verb method,
    const std::string& target,
    const std::string& body)
{
    net::io_context ioc;
    tcp::resolver resolver(ioc);
    beast::tcp_stream stream(ioc);

    const tcp::resolver::results_type results =
        resolver.resolve("127.0.0.1", std::to_string(port));
    stream.connect(results);

    http::request<http::string_body> request(method, target, 11);
    request.set(http::field::host, "127.0.0.1");
    request.set(http::field::user_agent, "reg_fsm_tests");
    if (!body.empty())
    {
        request.set(http::field::content_type, "application/json");
        request.body() = body;
    }
    request.prepare_payload();

    http::write(stream, request);

    beast::flat_buffer buffer;
    http::response<http::string_body> response;
    http::read(stream, buffer, response);

    boost::system::error_code ec;
    stream.socket().shutdown(tcp::socket::shutdown_both, ec);

    return response;
}

void StartCoreManager(Cfactory_mgr& mgr)
{
    assert(SUCCESS == mgr.RegisterFactory(new RegFactory(FAC_REG_FAC_ID)));
    assert(SUCCESS == mgr.RegisterFactory(new AuthFactory(FAC_AUTH_FAC_ID)));
    assert(SUCCESS == mgr.Start());
}

HttpServerConfig TestHttpConfig()
{
    HttpServerConfig config;
    config.host = "127.0.0.1";
    config.port = 0;
    config.syncTimeoutMs = 2000;
    return config;
}

unsigned int ReadJsonUnsigned(const json::value& value)
{
    if (value.is_uint64())
    {
        return static_cast<unsigned int>(value.as_uint64());
    }

    assert(value.is_int64());
    assert(value.as_int64() > 0);
    return static_cast<unsigned int>(value.as_int64());
}
}

void TestDefaultMsg()
{
    CMsg msg;
    assert(MSG_INIT == msg.type);
    assert(0 == msg.serviceId);
    assert(0 == msg.fsmId);
    assert(0 == msg.sessionId);
}

void TestInvalidTransition()
{
    RegFsm fsm;
    CMsg msg;
    msg.type = MSG_RESP;

    assert(INVALID_MSG == fsm.ProcessMsg(msg));
}

void TestAuthInvalidTransition()
{
    AuthFsm fsm;
    CMsg msg;
    msg.type = MSG_RESP;

    assert(INVALID_MSG == fsm.ProcessMsg(msg));
}

void TestManagerFactoryFlow()
{
    Cfactory_mgr mgr;
    assert(SUCCESS == mgr.RegisterFactory(new RegFactory(FAC_REG_FAC_ID)));
    assert(SUCCESS == mgr.RegisterFactory(new AuthFactory(FAC_AUTH_FAC_ID)));
    assert(SUCCESS == mgr.Start());

    CMsg msg;
    msg.serviceId = FAC_REG_FAC_ID;
    msg.type = MSG_INIT;
    assert(SUCCESS == mgr.SendMsg(msg));

    CMsg authMsg;
    authMsg.serviceId = FAC_AUTH_FAC_ID;
    authMsg.type = MSG_INIT;
    assert(SUCCESS == mgr.SendMsg(authMsg));

    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    mgr.Stop();
}

void TestHttpAsyncRegisterAndAuth()
{
    Cfactory_mgr mgr;
    StartCoreManager(mgr);

    HttpServer server(mgr, TestHttpConfig());
    assert(SUCCESS == server.Start());
    const unsigned short port = server.GetPort();
    assert(0 != port);

    http::response<http::string_body> regResponse =
        SendHttpRequest(port, http::verb::post, "/api/v1/register", "{}");
    assert(http::status::accepted == regResponse.result());

    http::response<http::string_body> authResponse =
        SendHttpRequest(port, http::verb::post, "/api/v1/auth", "{}");
    assert(http::status::accepted == authResponse.result());

    server.Stop();
    mgr.Stop();
}

void TestHttpSyncRegisterAndAuth()
{
    Cfactory_mgr mgr;
    StartCoreManager(mgr);

    HttpServer server(mgr, TestHttpConfig());
    assert(SUCCESS == server.Start());

    http::response<http::string_body> response =
        SendHttpRequest(server.GetPort(),
                        http::verb::post,
                        "/api/v1/register?wait=true",
                        "{\"sessionId\":123}");
    assert(http::status::ok == response.result());

    json::value body = json::parse(response.body());
    assert(body.is_object());
    assert("completed" == json::value_to<std::string>(body.as_object()["status"]));
    assert(123 == ReadJsonUnsigned(body.as_object()["sessionId"]));

    http::response<http::string_body> authResponse =
        SendHttpRequest(server.GetPort(),
                        http::verb::post,
                        "/api/v1/auth?wait=true",
                        "{\"sessionId\":124}");
    assert(http::status::ok == authResponse.result());

    json::value authBody = json::parse(authResponse.body());
    assert(authBody.is_object());
    assert("completed" == json::value_to<std::string>(authBody.as_object()["status"]));
    assert(124 == ReadJsonUnsigned(authBody.as_object()["sessionId"]));

    server.Stop();
    mgr.Stop();
}

void TestHttpErrors()
{
    Cfactory_mgr mgr;
    HttpServer server(mgr, TestHttpConfig());
    assert(SUCCESS == server.Start());
    const unsigned short port = server.GetPort();

    http::response<http::string_body> invalidJson =
        SendHttpRequest(port, http::verb::post, "/api/v1/register", "{bad");
    assert(http::status::bad_request == invalidJson.result());

    http::response<http::string_body> unknownPath =
        SendHttpRequest(port, http::verb::post, "/api/v1/missing", "{}");
    assert(http::status::not_found == unknownPath.result());

    http::response<http::string_body> invalidMethod =
        SendHttpRequest(port, http::verb::get, "/api/v1/register", "");
    assert(http::status::method_not_allowed == invalidMethod.result());

    server.Stop();
}

void TestHttpManagerUnavailable()
{
    Cfactory_mgr mgr;
    mgr.Stop();

    HttpServer server(mgr, TestHttpConfig());
    assert(SUCCESS == server.Start());

    http::response<http::string_body> response =
        SendHttpRequest(server.GetPort(), http::verb::post, "/api/v1/register", "{}");
    assert(http::status::service_unavailable == response.result());

    server.Stop();
}

int main()
{
    Logger::Instance().SetLevel(LogLevel::OFF);

    TestDefaultMsg();
    TestInvalidTransition();
    TestAuthInvalidTransition();
    TestManagerFactoryFlow();
    TestHttpAsyncRegisterAndAuth();
    TestHttpSyncRegisterAndAuth();
    TestHttpErrors();
    TestHttpManagerUnavailable();

    return 0;
}
