//
// Benchmark entry for FSM framework throughput and routing checks.
//

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <thread>
#include <vector>

#include "../reg_fsm/inc/AuthFactory.h"
#include "../reg_fsm/inc/CMsg.h"
#include "../reg_fsm/inc/Cfactory_mgr.h"
#include "../reg_fsm/inc/Logger.h"
#include "../reg_fsm/inc/RegFactory.h"
#include "../reg_fsm/inc/TimerManager.h"

namespace
{
const unsigned int PERF_FACTORY_ID = 7;
const unsigned int MULTI_FSM_FACTORY_ID = 8;

std::atomic<unsigned long long> g_processed(0);
std::atomic<unsigned long long> g_dispatchFailed(0);

struct BenchmarkConfig
{
    std::string caseName;
    unsigned long long messages;
    unsigned int producers;
    unsigned int fsmCount;
    unsigned int repeat;
    unsigned int timers;
    unsigned int timerDelayMs;
    LogLevel logLevel;

    BenchmarkConfig()
        : caseName("all"),
          messages(1000000),
          producers(4),
          fsmCount(FSM_NUM_IN_FAC),
          repeat(1),
          timers(10000),
          timerDelayMs(1),
          logLevel(LogLevel::OFF)
    {
    }
};

struct BenchmarkResult
{
    std::string caseName;
    unsigned int repeat;
    unsigned long long messages;
    unsigned int producers;
    unsigned int fsmCount;
    unsigned int timers;
    unsigned int timerDelayMs;
    unsigned long long sent;
    unsigned long long processed;
    unsigned long long failed;
    long long sendMs;
    long long totalMs;

    BenchmarkResult()
        : repeat(0),
          messages(0),
          producers(0),
          fsmCount(0),
          timers(0),
          timerDelayMs(0),
          sent(0),
          processed(0),
          failed(0),
          sendMs(0),
          totalMs(0)
    {
    }
};

class PerfFsm : public Cfsm
{
public:
    PerfFsm() : Cfsm(WORKING)
    {
    }

    void PrePrcMsg(CMsg& msg) override
    {
        (void)msg;
    }

    EerrNo ProcessMsg(CMsg& msg) override
    {
        (void)msg;
        g_processed.fetch_add(1);
        return SUCCESS;
    }

    void PostPrcMsg(CMsg& msg) override
    {
        (void)msg;
    }
};

class PerfFactory : public Cfactory
{
private:
    PerfFsm _fsm;

public:
    explicit PerfFactory(unsigned int facId) : Cfactory(facId)
    {
        this->_fsm.SetFsmId(1);
        this->_fsm.SetFactory(this);
    }

    Cfsm* CreateFsm() override
    {
        return nullptr;
    }

    EerrNo FacMsgPrc(CMsg& msg) override
    {
        msg.fsmId = this->_fsm.GetFsmId();
        this->_fsm.PrePrcMsg(msg);
        EerrNo ret = this->_fsm.ProcessMsg(msg);
        this->_fsm.PostPrcMsg(msg);
        return ret;
    }
};

class MultiPerfFactory : public Cfactory
{
public:
    explicit MultiPerfFactory(unsigned int facId) : Cfactory(facId)
    {
    }

    Cfsm* CreateFsm() override
    {
        return new PerfFsm();
    }

    bool InitFsms(unsigned int fsmCount)
    {
        for (unsigned int index = 0; index < fsmCount; ++index)
        {
            if (nullptr == AddFsm())
            {
                return false;
            }
        }

        return true;
    }

    EerrNo FacMsgPrc(CMsg& msg) override
    {
        Cfsm* fsm = FindFsm(msg.fsmId);
        return DispatchToFsm(fsm, msg);
    }
};

class CountingRegFactory : public RegFactory
{
public:
    explicit CountingRegFactory(unsigned int facId) : RegFactory(facId)
    {
    }

    EerrNo FacMsgPrc(CMsg& msg) override
    {
        EerrNo ret = RegFactory::FacMsgPrc(msg);
        if (SUCCESS == ret)
        {
            g_processed.fetch_add(1);
        }
        else
        {
            g_dispatchFailed.fetch_add(1);
        }

        return ret;
    }
};

class CountingAuthFactory : public AuthFactory
{
public:
    explicit CountingAuthFactory(unsigned int facId) : AuthFactory(facId)
    {
    }

    EerrNo FacMsgPrc(CMsg& msg) override
    {
        EerrNo ret = AuthFactory::FacMsgPrc(msg);
        if (SUCCESS == ret)
        {
            g_processed.fetch_add(1);
        }
        else
        {
            g_dispatchFailed.fetch_add(1);
        }

        return ret;
    }
};

bool StartsWith(const std::string& value, const std::string& prefix)
{
    return 0 == value.compare(0, prefix.size(), prefix);
}

bool ParseUnsignedLongLong(const std::string& text, unsigned long long& value)
{
    char* end = nullptr;
    const unsigned long long parsed = std::strtoull(text.c_str(), &end, 10);
    if ((nullptr == end) || ('\0' != *end) || text.empty())
    {
        return false;
    }

    value = parsed;
    return true;
}

bool ParseUnsignedInt(const std::string& text, unsigned int& value)
{
    unsigned long long parsed = 0;
    if (!ParseUnsignedLongLong(text, parsed) ||
        parsed > std::numeric_limits<unsigned int>::max())
    {
        return false;
    }

    value = static_cast<unsigned int>(parsed);
    return true;
}

bool ParseLogLevel(const std::string& text, LogLevel& level)
{
    if ("off" == text)
    {
        level = LogLevel::OFF;
    }
    else if ("debug" == text)
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
    else
    {
        return false;
    }

    return true;
}

void PrintUsage(const char* program)
{
    std::cout << "Usage: " << program
              << " [--case=all|noop|multi_fsm|concurrent|real_flow|timer]"
              << " [--messages=N]"
              << " [--producers=N]"
              << " [--fsm-count=N]"
              << " [--repeat=N]"
              << " [--timers=N]"
              << " [--timer-delay-ms=N]"
              << " [--log-level=off|debug|info|warn|error]"
              << std::endl;
}

bool ParseArgs(int argc, char* argv[], BenchmarkConfig& config)
{
    for (int index = 1; index < argc; ++index)
    {
        const std::string arg(argv[index]);
        if (("--help" == arg) || ("-h" == arg))
        {
            PrintUsage(argv[0]);
            return false;
        }
        else if (StartsWith(arg, "--case="))
        {
            config.caseName = arg.substr(7);
        }
        else if (StartsWith(arg, "--messages="))
        {
            if (!ParseUnsignedLongLong(arg.substr(11), config.messages))
            {
                return false;
            }
        }
        else if (StartsWith(arg, "--producers="))
        {
            if (!ParseUnsignedInt(arg.substr(12), config.producers))
            {
                return false;
            }
        }
        else if (StartsWith(arg, "--fsm-count="))
        {
            if (!ParseUnsignedInt(arg.substr(12), config.fsmCount))
            {
                return false;
            }
        }
        else if (StartsWith(arg, "--repeat="))
        {
            if (!ParseUnsignedInt(arg.substr(9), config.repeat))
            {
                return false;
            }
        }
        else if (StartsWith(arg, "--timers="))
        {
            if (!ParseUnsignedInt(arg.substr(9), config.timers))
            {
                return false;
            }
        }
        else if (StartsWith(arg, "--timer-delay-ms="))
        {
            if (!ParseUnsignedInt(arg.substr(17), config.timerDelayMs))
            {
                return false;
            }
        }
        else if (StartsWith(arg, "--log-level="))
        {
            if (!ParseLogLevel(arg.substr(12), config.logLevel))
            {
                return false;
            }
        }
        else
        {
            return false;
        }
    }

    if ((0 == config.messages) ||
        (0 == config.producers) ||
        (0 == config.fsmCount) ||
        (config.fsmCount > FSM_NUM_IN_FAC) ||
        (0 == config.repeat) ||
        (0 == config.timers))
    {
        return false;
    }

    return ("all" == config.caseName) ||
           ("noop" == config.caseName) ||
           ("multi_fsm" == config.caseName) ||
           ("concurrent" == config.caseName) ||
           ("real_flow" == config.caseName) ||
           ("timer" == config.caseName);
}

long long ElapsedMs(std::chrono::steady_clock::time_point begin,
                    std::chrono::steady_clock::time_point end)
{
    const auto elapsed = end - begin;
    const long long ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    if ((0 == ms) &&
        (std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count() > 0))
    {
        return 1;
    }

    return ms;
}

double CalcQps(unsigned long long count, long long elapsedMs)
{
    return (elapsedMs > 0)
        ? (count * 1000.0 / static_cast<double>(elapsedMs))
        : 0.0;
}

void WaitForProcessed(unsigned long long target)
{
    while (g_processed.load() < target)
    {
        std::this_thread::yield();
    }
}

void PrintResult(const BenchmarkResult& result)
{
    std::cout << std::fixed << std::setprecision(2)
              << "[benchmark] case=" << result.caseName
              << " repeat=" << result.repeat << std::endl
              << "  workload messages=" << result.messages
              << " producers=" << result.producers
              << " fsm_count=" << result.fsmCount
              << " timers=" << result.timers
              << " timer_delay_ms=" << result.timerDelayMs << std::endl
              << "  result   sent=" << result.sent
              << " processed=" << result.processed
              << " failed=" << result.failed << std::endl
              << "  elapsed  send_ms=" << result.sendMs
              << " total_ms=" << result.totalMs << std::endl
              << "  qps      send=" << CalcQps(result.sent, result.sendMs)
              << " total=" << CalcQps(result.processed, result.totalMs)
              << std::endl
              << std::endl;
}

BenchmarkResult RunNoop(const BenchmarkConfig& config, unsigned int repeat)
{
    BenchmarkResult result;
    result.caseName = "noop";
    result.repeat = repeat;
    result.messages = config.messages;
    result.producers = 1;
    result.fsmCount = 1;

    g_processed.store(0);

    Cfactory_mgr mgr;
    if ((SUCCESS != mgr.RegisterFactory(new PerfFactory(PERF_FACTORY_ID))) ||
        (SUCCESS != mgr.Start()))
    {
        result.failed = 1;
        return result;
    }

    CMsg msg;
    msg.serviceId = PERF_FACTORY_ID;
    msg.type = MSG_INIT;

    const auto sendBegin = std::chrono::steady_clock::now();
    for (unsigned long long index = 0; index < config.messages; ++index)
    {
        if (SUCCESS == mgr.SendMsg(msg))
        {
            ++result.sent;
        }
        else
        {
            ++result.failed;
        }
    }
    const auto sendEnd = std::chrono::steady_clock::now();

    WaitForProcessed(result.sent);
    const auto processEnd = std::chrono::steady_clock::now();

    mgr.Stop();

    result.processed = g_processed.load();
    result.sendMs = ElapsedMs(sendBegin, sendEnd);
    result.totalMs = ElapsedMs(sendBegin, processEnd);
    return result;
}

BenchmarkResult RunMultiFsm(const BenchmarkConfig& config, unsigned int repeat)
{
    BenchmarkResult result;
    result.caseName = "multi_fsm";
    result.repeat = repeat;
    result.messages = config.messages;
    result.producers = 1;
    result.fsmCount = config.fsmCount;

    g_processed.store(0);

    Cfactory_mgr mgr;
    MultiPerfFactory* factory = new MultiPerfFactory(MULTI_FSM_FACTORY_ID);
    if ((SUCCESS != mgr.RegisterFactory(factory)) ||
        !factory->InitFsms(config.fsmCount) ||
        (SUCCESS != mgr.Start()))
    {
        result.failed = 1;
        return result;
    }

    CMsg msg;
    msg.serviceId = MULTI_FSM_FACTORY_ID;
    msg.type = MSG_REQ;

    const auto sendBegin = std::chrono::steady_clock::now();
    for (unsigned long long index = 0; index < config.messages; ++index)
    {
        msg.fsmId = static_cast<unsigned int>((index % config.fsmCount) + 1);
        if (SUCCESS == mgr.SendMsg(msg))
        {
            ++result.sent;
        }
        else
        {
            ++result.failed;
        }
    }
    const auto sendEnd = std::chrono::steady_clock::now();

    WaitForProcessed(result.sent);
    const auto processEnd = std::chrono::steady_clock::now();

    mgr.Stop();

    result.processed = g_processed.load();
    result.sendMs = ElapsedMs(sendBegin, sendEnd);
    result.totalMs = ElapsedMs(sendBegin, processEnd);
    return result;
}

BenchmarkResult RunConcurrent(const BenchmarkConfig& config, unsigned int repeat)
{
    BenchmarkResult result;
    result.caseName = "concurrent";
    result.repeat = repeat;
    result.messages = config.messages;
    result.producers = config.producers;
    result.fsmCount = 1;

    g_processed.store(0);

    Cfactory_mgr mgr;
    if ((SUCCESS != mgr.RegisterFactory(new PerfFactory(PERF_FACTORY_ID))) ||
        (SUCCESS != mgr.Start()))
    {
        result.failed = 1;
        return result;
    }

    std::atomic<unsigned long long> sent(0);
    std::atomic<unsigned long long> failed(0);

    const auto sendBegin = std::chrono::steady_clock::now();
    std::vector<std::thread> producers;
    for (unsigned int producerIndex = 0; producerIndex < config.producers; ++producerIndex)
    {
        const unsigned long long baseCount = config.messages / config.producers;
        const unsigned long long extra = producerIndex < (config.messages % config.producers)
            ? 1
            : 0;
        const unsigned long long messageCount = baseCount + extra;

        producers.push_back(std::thread([&mgr, &sent, &failed, messageCount]() {
            CMsg msg;
            msg.serviceId = PERF_FACTORY_ID;
            msg.type = MSG_INIT;

            for (unsigned long long index = 0; index < messageCount; ++index)
            {
                if (SUCCESS == mgr.SendMsg(msg))
                {
                    sent.fetch_add(1);
                }
                else
                {
                    failed.fetch_add(1);
                }
            }
        }));
    }

    for (std::thread& producer : producers)
    {
        producer.join();
    }
    const auto sendEnd = std::chrono::steady_clock::now();

    result.sent = sent.load();
    result.failed = failed.load();

    WaitForProcessed(result.sent);
    const auto processEnd = std::chrono::steady_clock::now();

    mgr.Stop();

    result.processed = g_processed.load();
    result.sendMs = ElapsedMs(sendBegin, sendEnd);
    result.totalMs = ElapsedMs(sendBegin, processEnd);
    return result;
}

BenchmarkResult RunRealFlow(const BenchmarkConfig& config, unsigned int repeat)
{
    BenchmarkResult result;
    result.caseName = "real_flow";
    result.repeat = repeat;

    const unsigned long long maxFlowPairs = FSM_NUM_IN_FAC;
    const unsigned long long flowPairs =
        (config.messages < maxFlowPairs) ? config.messages : maxFlowPairs;
    const unsigned long long expectedEvents = flowPairs * 11;

    result.messages = flowPairs * 2;
    result.producers = 1;
    result.fsmCount = static_cast<unsigned int>(flowPairs);

    g_processed.store(0);
    g_dispatchFailed.store(0);

    Cfactory_mgr mgr;
    if ((SUCCESS != mgr.RegisterFactory(new CountingRegFactory(FAC_REG_FAC_ID))) ||
        (SUCCESS != mgr.RegisterFactory(new CountingAuthFactory(FAC_AUTH_FAC_ID))) ||
        (SUCCESS != mgr.Start()))
    {
        result.failed = 1;
        return result;
    }

    CMsg regMsg;
    regMsg.serviceId = FAC_REG_FAC_ID;
    regMsg.type = MSG_INIT;

    CMsg authMsg;
    authMsg.serviceId = FAC_AUTH_FAC_ID;
    authMsg.type = MSG_INIT;

    const auto sendBegin = std::chrono::steady_clock::now();
    for (unsigned long long index = 0; index < flowPairs; ++index)
    {
        if (SUCCESS == mgr.SendMsg(regMsg))
        {
            ++result.sent;
        }
        else
        {
            ++result.failed;
        }

        if (SUCCESS == mgr.SendMsg(authMsg))
        {
            ++result.sent;
        }
        else
        {
            ++result.failed;
        }
    }
    const auto sendEnd = std::chrono::steady_clock::now();

    WaitForProcessed(expectedEvents);
    const auto processEnd = std::chrono::steady_clock::now();

    mgr.Stop();

    result.processed = g_processed.load();
    result.failed += g_dispatchFailed.load();
    result.sendMs = ElapsedMs(sendBegin, sendEnd);
    result.totalMs = ElapsedMs(sendBegin, processEnd);
    return result;
}

BenchmarkResult RunTimer(const BenchmarkConfig& config, unsigned int repeat)
{
    BenchmarkResult result;
    result.caseName = "timer";
    result.repeat = repeat;
    result.messages = config.timers;
    result.producers = 1;
    result.timers = config.timers;
    result.timerDelayMs = config.timerDelayMs;

    g_processed.store(0);

    TimerManager timerManager([](const CMsg& msg) {
        (void)msg;
        g_processed.fetch_add(1);
        return SUCCESS;
    });

    CMsg msg;
    msg.serviceId = PERF_FACTORY_ID;
    msg.type = MSG_TIMEOUT;

    const auto sendBegin = std::chrono::steady_clock::now();
    for (unsigned int index = 0; index < config.timers; ++index)
    {
        try
        {
            if (0 != timerManager.StartTimer(config.timerDelayMs, msg))
            {
                ++result.sent;
            }
            else
            {
                ++result.failed;
            }
        }
        catch (const std::exception&)
        {
            ++result.failed;
            break;
        }
    }
    const auto sendEnd = std::chrono::steady_clock::now();

    WaitForProcessed(result.sent);
    const auto processEnd = std::chrono::steady_clock::now();

    timerManager.StopAndJoin();

    result.processed = g_processed.load();
    result.sendMs = ElapsedMs(sendBegin, sendEnd);
    result.totalMs = ElapsedMs(sendBegin, processEnd);
    return result;
}

bool ShouldRun(const BenchmarkConfig& config, const std::string& caseName)
{
    return ("all" == config.caseName) || (caseName == config.caseName);
}

bool ValidateResult(const BenchmarkResult& result)
{
    return (0 == result.failed) && (result.processed >= result.sent);
}
}

int main(int argc, char* argv[])
{
    BenchmarkConfig config;
    if (!ParseArgs(argc, argv, config))
    {
        PrintUsage(argv[0]);
        return 1;
    }

    Logger::Instance().SetLevel(config.logLevel);

    bool ok = true;
    for (unsigned int repeat = 1; repeat <= config.repeat; ++repeat)
    {
        if (ShouldRun(config, "noop"))
        {
            const BenchmarkResult result = RunNoop(config, repeat);
            PrintResult(result);
            ok = ValidateResult(result) && ok;
        }

        if (ShouldRun(config, "multi_fsm"))
        {
            const BenchmarkResult result = RunMultiFsm(config, repeat);
            PrintResult(result);
            ok = ValidateResult(result) && ok;
        }

        if (ShouldRun(config, "concurrent"))
        {
            const BenchmarkResult result = RunConcurrent(config, repeat);
            PrintResult(result);
            ok = ValidateResult(result) && ok;
        }

        if (ShouldRun(config, "real_flow"))
        {
            const BenchmarkResult result = RunRealFlow(config, repeat);
            PrintResult(result);
            ok = ValidateResult(result) && ok;
        }

        if (ShouldRun(config, "timer"))
        {
            const BenchmarkResult result = RunTimer(config, repeat);
            PrintResult(result);
            ok = ValidateResult(result) && ok;
        }
    }

    return ok ? 0 : 2;
}
