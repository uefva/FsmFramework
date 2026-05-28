//
// Created by MR on 2026/5/28.
//

#include "../inc/Cfactory_mgr.h"

#include <chrono>
#include <iostream>
#include <utility>

Cfactory_mgr::Cfactory_mgr()
    : _stopped(false), _running(false), _next_timer_id(1)
{
    this->_fac_list.reserve(FAC_NUM_IN_MGR_MAX);
}

Cfactory_mgr::~Cfactory_mgr()
{
    // Stop accepting new work before reclaiming worker threads, timers, and factories.
    Stop();
    Join();
    StopAllTimers();

    for (std::thread& timerThread : this->_timer_threads)
    {
        if (timerThread.joinable())
        {
            timerThread.join();
        }
    }

    {
        std::lock_guard<std::mutex> guard(this->_fac_lock);
        for (Cfactory* factory : this->_fac_list)
        {
            delete factory;
        }

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
        std::cout << "Cfactory_mgr::RegisterFactory failed, manager is full" << std::endl;
        return ERROR;
    }

    if (nullptr != FindFactory(factory->GetFacId()))
    {
        std::cout << "Cfactory_mgr::RegisterFactory failed, duplicated facId="
                  << factory->GetFacId() << std::endl;
        return ERROR;
    }

    factory->SetFacMgr(this);
    this->_fac_list.push_back(factory);

    std::cout << "Cfactory_mgr::RegisterFactory facId="
              << factory->GetFacId() << std::endl;

    return SUCCESS;
}

EerrNo Cfactory_mgr::SendMsg(const CMsg& msg)
{
    {
        // Hold the queue lock only while pushing the message.
        std::lock_guard<std::mutex> guard(this->_pump_lock);
        if (this->_stopped)
        {
            return ERROR;
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
    std::shared_ptr<CTimerCtrl> timerCtrl;

    {
        // Store a timer control block so StopTimer can cancel posting.
        std::lock_guard<std::mutex> guard(this->_timer_lock);
        timerCtrl.reset(new CTimerCtrl(this->_next_timer_id++));
        this->_timer_list.push_back(timerCtrl);
    }

    // Prototype timer: one sleeping thread per timer. A future TimerManager can replace it.
    std::thread timerThread([this, timeoutMs, timeoutMsg, timerCtrl]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(timeoutMs));
        if (timerCtrl->active.load())
        {
            SendMsg(timeoutMsg);
        }
    });

    {
        std::lock_guard<std::mutex> guard(this->_timer_lock);
        this->_timer_threads.push_back(std::move(timerThread));
    }

    return timerCtrl->id;
}

EerrNo Cfactory_mgr::StopTimer(WS_TIMER_ID timerId)
{
    std::lock_guard<std::mutex> guard(this->_timer_lock);

    for (std::shared_ptr<CTimerCtrl>& timerCtrl : this->_timer_list)
    {
        if ((nullptr != timerCtrl) && (timerCtrl->id == timerId))
        {
            timerCtrl->active.store(false);
            return SUCCESS;
        }
    }

    return ERROR;
}

void Cfactory_mgr::StopAllTimers()
{
    std::lock_guard<std::mutex> guard(this->_timer_lock);

    for (std::shared_ptr<CTimerCtrl>& timerCtrl : this->_timer_list)
    {
        if (nullptr != timerCtrl)
        {
            timerCtrl->active.store(false);
        }
    }
}

Cfactory* Cfactory_mgr::FindFactory(unsigned int serviceId)
{
    for (Cfactory* factory : this->_fac_list)
    {
        if ((nullptr != factory) && (factory->GetFacId() == serviceId))
        {
            return factory;
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
        std::cout << "Cfactory_mgr::DispatchMsg unknown serviceId="
                  << msg.serviceId << std::endl;
        return ERROR;
    }

    return factory->FacMsgPrc(msg);
}
