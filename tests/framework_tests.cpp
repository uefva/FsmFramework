//
// Created by MR on 2026/5/28.
//

#include <cassert>
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

#include "../reg_fsm/inc/AuthFactory.h"
#include "../reg_fsm/inc/AuthFsm.h"
#include "../reg_fsm/inc/CMsg.h"
#include "../reg_fsm/inc/Cfactory_mgr.h"
#include "../reg_fsm/inc/RegFactory.h"
#include "../reg_fsm/inc/RegFsm.h"

namespace
{
const unsigned int PERF_FACTORY_ID = 7;
const unsigned int MULTI_FSM_FACTORY_ID = 8;
const unsigned int PERF_MSG_COUNT = 10000000;
const unsigned int MULTI_FSM_COUNT = FSM_NUM_IN_FAC;
const unsigned int PRODUCER_THREAD_COUNT = 4;
const unsigned int REAL_FLOW_COUNT = 8;
std::atomic<unsigned int> g_perfProcessed(0);

class PerfFsm : public Cfsm
{
public:
    PerfFsm() : Cfsm(WORKING)
    {
    }

    void PrePrcMsg(CMsg& pBuf) override
    {
        (void)pBuf;
    }

    EerrNo ProcessMsg(CMsg& pMsg) override
    {
        (void)pMsg;
        g_perfProcessed.fetch_add(1);
        return SUCCESS;
    }

    void PostPrcMsg(CMsg& pBuf) override
    {
        (void)pBuf;
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

    void InitFsms(unsigned int fsmCount)
    {
        for (unsigned int index = 0; index < fsmCount; ++index)
        {
            assert(nullptr != AddFsm());
        }
    }

    EerrNo FacMsgPrc(CMsg& msg) override
    {
        Cfsm* fsm = FindFsm(msg.fsmId);
        return DispatchToFsm(fsm, msg);
    }
};

double CalcQps(unsigned int count, long long elapsedMs)
{
    return (elapsedMs > 0)
        ? (count * 1000.0 / static_cast<double>(elapsedMs))
        : 0.0;
}
}

void TestDefaultMsg()
{
    // Verify the default event type and routing fields.
    CMsg msg;
    assert(MSG_INIT == msg.type);
    assert(0 == msg.serviceId);
    assert(0 == msg.fsmId);
    assert(0 == msg.sessionId);
}

void TestInvalidTransition()
{
    // RegFsm starts from IDLE, so MSG_RESP is an invalid transition.
    RegFsm fsm;
    CMsg msg;
    msg.type = MSG_RESP;

    assert(ERROR == fsm.ProcessMsg(msg));
}

void TestAuthInvalidTransition()
{
    AuthFsm fsm;
    CMsg msg;
    msg.type = MSG_RESP;

    assert(ERROR == fsm.ProcessMsg(msg));
}

void TestManagerFactoryFlow()
{
    // Verify that the manager -> factory -> FSM pipeline can be driven.
    Cfactory_mgr mgr;
    assert(SUCCESS == mgr.RegisterFactory(new RegFactory(1)));
    assert(SUCCESS == mgr.RegisterFactory(new AuthFactory(3)));
    assert(SUCCESS == mgr.Start());

    CMsg msg;
    msg.serviceId = 1;
    msg.type = MSG_INIT;
    assert(SUCCESS == mgr.SendMsg(msg));

    CMsg authMsg;
    authMsg.serviceId = 3;
    authMsg.type = MSG_INIT;
    assert(SUCCESS == mgr.SendMsg(authMsg));

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    mgr.Stop();
    mgr.Join();
}

void StressTestMessageDispatch()
{
    g_perfProcessed.store(0);

    Cfactory_mgr mgr;
    assert(SUCCESS == mgr.RegisterFactory(new PerfFactory(PERF_FACTORY_ID)));
    assert(SUCCESS == mgr.Start());

    CMsg msg;
    msg.serviceId = PERF_FACTORY_ID;
    msg.type = MSG_INIT;

    const auto sendBegin = std::chrono::steady_clock::now();
    for (unsigned int index = 0; index < PERF_MSG_COUNT; ++index)
    {
        assert(SUCCESS == mgr.SendMsg(msg));
    }
    const auto sendEnd = std::chrono::steady_clock::now();

    const auto processBegin = sendBegin;
    while (g_perfProcessed.load() < PERF_MSG_COUNT)
    {
        std::this_thread::yield();
    }
    const auto processEnd = std::chrono::steady_clock::now();

    mgr.Stop();
    mgr.Join();

    const auto sendMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(sendEnd - sendBegin).count();
    const auto totalMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(processEnd - processBegin).count();

    std::cout << "[StressTest] messages=" << PERF_MSG_COUNT
              << " send_ms=" << sendMs
              << " total_ms=" << totalMs
              << " send_qps=" << CalcQps(PERF_MSG_COUNT, sendMs)
              << " total_qps=" << CalcQps(PERF_MSG_COUNT, totalMs)
              << std::endl;
}

void StressTestRealRegAuthFlow()
{
    Cfactory_mgr mgr;
    assert(SUCCESS == mgr.RegisterFactory(new RegFactory(FAC_REG_FAC_ID)));
    assert(SUCCESS == mgr.RegisterFactory(new AuthFactory(FAC_AUTH_FAC_ID)));
    assert(SUCCESS == mgr.Start());

    CMsg regMsg;
    regMsg.serviceId = FAC_REG_FAC_ID;
    regMsg.type = MSG_INIT;

    CMsg authMsg;
    authMsg.serviceId = FAC_AUTH_FAC_ID;
    authMsg.type = MSG_INIT;

    const unsigned int totalInitialMessages = REAL_FLOW_COUNT * 2;
    const auto begin = std::chrono::steady_clock::now();
    for (unsigned int index = 0; index < REAL_FLOW_COUNT; ++index)
    {
        assert(SUCCESS == mgr.SendMsg(regMsg));
        assert(SUCCESS == mgr.SendMsg(authMsg));
    }
    const auto sendEnd = std::chrono::steady_clock::now();

    // Real Reg/Auth flows include logs and timer-delayed close/timeout events.
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    const auto end = std::chrono::steady_clock::now();

    mgr.Stop();
    mgr.Join();

    const auto sendMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(sendEnd - begin).count();
    const auto totalMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();

    std::cout << "[StressTestRealRegAuth] initial_messages=" << totalInitialMessages
              << " reg_flows=" << REAL_FLOW_COUNT
              << " auth_flows=" << REAL_FLOW_COUNT
              << " send_ms=" << sendMs
              << " observed_ms=" << totalMs
              << std::endl;
}

void StressTestMultiFsmRouting()
{
    g_perfProcessed.store(0);

    Cfactory_mgr mgr;
    MultiPerfFactory* factory = new MultiPerfFactory(MULTI_FSM_FACTORY_ID);
    assert(SUCCESS == mgr.RegisterFactory(factory));
    factory->InitFsms(MULTI_FSM_COUNT);
    assert(SUCCESS == mgr.Start());

    CMsg msg;
    msg.serviceId = MULTI_FSM_FACTORY_ID;
    msg.type = MSG_REQ;

    const auto sendBegin = std::chrono::steady_clock::now();
    for (unsigned int index = 0; index < PERF_MSG_COUNT; ++index)
    {
        msg.fsmId = (index % MULTI_FSM_COUNT) + 1;
        assert(SUCCESS == mgr.SendMsg(msg));
    }
    const auto sendEnd = std::chrono::steady_clock::now();

    while (g_perfProcessed.load() < PERF_MSG_COUNT)
    {
        std::this_thread::yield();
    }
    const auto processEnd = std::chrono::steady_clock::now();

    mgr.Stop();
    mgr.Join();

    const auto sendMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(sendEnd - sendBegin).count();
    const auto totalMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(processEnd - sendBegin).count();

    std::cout << "[StressTestMultiFsm] messages=" << PERF_MSG_COUNT
              << " fsm_count=" << MULTI_FSM_COUNT
              << " send_ms=" << sendMs
              << " total_ms=" << totalMs
              << " send_qps=" << CalcQps(PERF_MSG_COUNT, sendMs)
              << " total_qps=" << CalcQps(PERF_MSG_COUNT, totalMs)
              << std::endl;
}

void StressTestConcurrentSend()
{
    g_perfProcessed.store(0);

    Cfactory_mgr mgr;
    assert(SUCCESS == mgr.RegisterFactory(new PerfFactory(PERF_FACTORY_ID)));
    assert(SUCCESS == mgr.Start());

    const unsigned int messagesPerThread = PERF_MSG_COUNT / PRODUCER_THREAD_COUNT;
    const unsigned int totalMessages = messagesPerThread * PRODUCER_THREAD_COUNT;

    const auto sendBegin = std::chrono::steady_clock::now();
    std::vector<std::thread> producers;
    for (unsigned int threadIndex = 0; threadIndex < PRODUCER_THREAD_COUNT; ++threadIndex)
    {
        producers.push_back(std::thread([&mgr, messagesPerThread]() {
            CMsg msg;
            msg.serviceId = PERF_FACTORY_ID;
            msg.type = MSG_INIT;
            for (unsigned int index = 0; index < messagesPerThread; ++index)
            {
                assert(SUCCESS == mgr.SendMsg(msg));
            }
        }));
    }

    for (std::thread& producer : producers)
    {
        producer.join();
    }
    const auto sendEnd = std::chrono::steady_clock::now();

    while (g_perfProcessed.load() < totalMessages)
    {
        std::this_thread::yield();
    }
    const auto processEnd = std::chrono::steady_clock::now();

    mgr.Stop();
    mgr.Join();

    const auto sendMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(sendEnd - sendBegin).count();
    const auto totalMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(processEnd - sendBegin).count();

    std::cout << "[StressTestConcurrentSend] messages=" << totalMessages
              << " producers=" << PRODUCER_THREAD_COUNT
              << " send_ms=" << sendMs
              << " total_ms=" << totalMs
              << " send_qps=" << CalcQps(totalMessages, sendMs)
              << " total_qps=" << CalcQps(totalMessages, totalMs)
              << std::endl;
}

int main()
{
    // Basic message model test: verifies CMsg default event type and routing fields.
    TestDefaultMsg();

    // RegFsm negative test: verifies an invalid event is rejected in the IDLE state.
    TestInvalidTransition();

    // AuthFsm negative test: verifies an invalid event is rejected in the IDLE state.
    TestAuthInvalidTransition();

    // End-to-end smoke test: verifies manager -> factory -> FSM message dispatch works.
    TestManagerFactoryFlow();

    // Baseline performance test: measures single-producer message dispatch throughput.
    StressTestMessageDispatch();

    // Real-flow performance observation: runs RegFsm/AuthFsm with actions, logs, and timers.
    StressTestRealRegAuthFlow();

    // Multi-instance routing test: measures FindFsm/DispatchToFsm cost across many FSMs.
    StressTestMultiFsmRouting();

    // Concurrent producer test: measures SendMsg under multi-threaded producer contention.
    StressTestConcurrentSend();

    return 0;
}
