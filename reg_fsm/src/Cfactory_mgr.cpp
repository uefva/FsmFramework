//
// Created by MR on 2026/5/28.
//

#include "../inc/Cfactory_mgr.h"

#include <chrono>
#include <utility>

#include "../inc/Logger.h"

Cfactory_mgr::Cfactory_mgr()
    : _stopped(false), _running(false),
      _timerManager([this](const CMsg& msg) { return this->SendMsg(msg); })
{
    this->_fac_list.reserve(FAC_NUM_IN_MGR_MAX);
}

Cfactory_mgr::~Cfactory_mgr()
{
    // Stop accepting new work before reclaiming worker threads, timers, and factories.
    Stop();
    _timerManager.StopAndJoin();

    {
        std::lock_guard<std::mutex> guard(this->_fac_lock);
        this->_fac_list.clear();
    }
}

EerrNo Cfactory_mgr::RegisterFactory(Cfactory* factory)
{
    if (nullptr == factory)
    {
        return ERROR;
    }

    // Registering a factory mutates _fac_list and must be protected.
    std::lock_guard<std::mutex> guard(this->_fac_lock);

    if (this->_fac_list.size() >= FAC_NUM_IN_MGR_MAX)
    {
        LOG_ERROR("Cfactory_mgr", "RegisterFactory failed, manager is full");
        return ERROR;
    }

    if (nullptr != FindFactory(factory->GetFacId()))
    {
        LOG_ERROR("Cfactory_mgr", "RegisterFactory failed, duplicated facId="
                                  << factory->GetFacId());
        return ERROR;
    }

    factory->SetFacMgr(this);
    this->_fac_list.emplace_back(factory);

    LOG_DEBUG("Cfactory_mgr", "RegisterFactory facId=" << factory->GetFacId());

    return SUCCESS;
}

EerrNo Cfactory_mgr::SendMsg(const CMsg& msg)
{
    {
        // Hold the queue lock only while pushing the message.
        std::lock_guard<std::mutex> guard(this->_pump_lock);
        if (this->_stopped)
        {
            return INVALID_STATE;
        }

        this->_pump.push(msg);
    }

    // Wake the worker if Run() is blocked on an empty queue.
    this->_pump_cv.notify_one();
    return SUCCESS;
}

EerrNo Cfactory_mgr::PumpOnce()
{
    CMsg msg;

    {
        // Pop the message first, then release the lock before dispatching it.
        // Business logic may post more messages and should not block the whole pump.
        std::lock_guard<std::mutex> guard(this->_pump_lock);
        if (this->_pump.empty())
        {
            return SUCCESS;
        }

        msg = this->_pump.front();
        this->_pump.pop();
    }

    return DispatchMsg(msg);
}

void Cfactory_mgr::RunUntilEmpty()
{
    // Drain messages that are already queued; do not wait for future timer events.
    while (true)
    {
        {
            std::lock_guard<std::mutex> guard(this->_pump_lock);
            if (this->_pump.empty())
            {
                break;
            }
        }

        PumpOnce();
    }
}

void Cfactory_mgr::Run()
{
    // Blocking message loop, normally executed by _worker_thread.
    while (true)
    {
        CMsg msg;

        {
            std::unique_lock<std::mutex> guard(this->_pump_lock);
            this->_pump_cv.wait(guard, [this]() {
                return this->_stopped || !this->_pump.empty();
            });

            if (this->_stopped && this->_pump.empty())
            {
                break;
            }

            msg = this->_pump.front();
            this->_pump.pop();
        }

        DispatchMsg(msg);
    }

    {
        std::lock_guard<std::mutex> guard(this->_pump_lock);
        this->_running = false;
    }
}

EerrNo Cfactory_mgr::Start()
{
    std::lock_guard<std::mutex> guard(this->_pump_lock);

    if (this->_running)
    {
        return ERROR;
    }

    if (this->_worker_thread.joinable())
    {
        return ERROR;
    }

    this->_stopped = false;
    this->_running = true;
    this->_worker_thread = std::thread(&Cfactory_mgr::Run, this);

    return SUCCESS;
}

void Cfactory_mgr::Stop()
{
    {
        // After Stop, SendMsg fails and Run exits after the queue is drained.
        std::lock_guard<std::mutex> guard(this->_pump_lock);
        this->_stopped = true;
    }

    this->_pump_cv.notify_all();
    this->Join();
}

void Cfactory_mgr::Join()
{
    if (this->_worker_thread.joinable())
    {
        this->_worker_thread.join();
    }
}

WS_TIMER_ID Cfactory_mgr::StartTimer(unsigned int timeoutMs, const CMsg& timeoutMsg)
{
    return this->_timerManager.StartTimer(timeoutMs, timeoutMsg);
}

EerrNo Cfactory_mgr::StopTimer(WS_TIMER_ID timerId)
{
    return this->_timerManager.StopTimer(timerId);
}

void Cfactory_mgr::StopAllTimers()
{
    this->_timerManager.StopAllTimers();
}

Cfactory* Cfactory_mgr::FindFactory(unsigned int serviceId)
{
    for (auto& factory : this->_fac_list)
    {
        if (factory && (factory->GetFacId() == serviceId))
        {
            return factory.get();
        }
    }

    return nullptr;
}

EerrNo Cfactory_mgr::DispatchMsg(CMsg& msg)
{
    Cfactory* factory = nullptr;

    {
        // Only protect the factory lookup. Dispatch can run business code and may be slow.
        std::lock_guard<std::mutex> guard(this->_fac_lock);
        factory = FindFactory(msg.serviceId);
    }

    if (nullptr == factory)
    {
        LOG_WARN("Cfactory_mgr", "DispatchMsg unknown serviceId="
                                 << msg.serviceId
                                 << " event=" << MsgTypeToString(msg.type));
        return INVALID_MSG;
    }

    return factory->FacMsgPrc(msg);
}
