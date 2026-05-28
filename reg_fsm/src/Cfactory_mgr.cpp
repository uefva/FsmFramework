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
    this->_fac_list.reserve(FAC_NUM_IN_MGR);
}

Cfactory_mgr::~Cfactory_mgr()
{
    // 析构时先停止接收新消息和所有定时器，再回收线程与工厂。
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

    // 注册工厂会修改 _fac_list，需要加锁保护。
    std::lock_guard<std::mutex> guard(this->_fac_lock);

    if (this->_fac_list.size() >= FAC_NUM_IN_MGR)
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
        // 只在入队时持有队列锁，避免调用方长时间阻塞。
        std::lock_guard<std::mutex> guard(this->_pump_lock);
        if (this->_stopped)
        {
            return ERROR;
        }

        this->_pump.push(msg);
    }

    // 唤醒阻塞在 Run() 中的消息泵线程。
    this->_pump_cv.notify_one();
    return SUCCESS;
}

EerrNo Cfactory_mgr::PumpOnce()
{
    CMsg msg;

    {
        // 先把消息从队列中取出，再释放锁执行分发。
        // 这样业务处理过程中继续 SendMsg 不会阻塞整个消息泵。
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
    // 只处理当前已经在队列中的消息，不等待未来新消息或定时器消息。
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
    // 阻塞式消息循环，通常由后台线程调用。
    while (true)
    {
        CMsg msg;

        {
            std::unique_lock<std::mutex> guard(this->_pump_lock);
            // 队列为空时阻塞等待；收到 Stop 或新消息时被唤醒。
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
        // Stop 后 SendMsg 会失败，Run 会在队列清空后退出。
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
        // 保存 timer 控制块，StopTimer 可以通过 active 标志取消投递。
        std::lock_guard<std::mutex> guard(this->_timer_lock);
        timerCtrl.reset(new CTimerCtrl(this->_next_timer_id++));
        this->_timer_list.push_back(timerCtrl);
    }

    // 原型版定时器：每个定时器使用一个线程 sleep，到期后投递消息。
    // 后续可替换为统一 TimerManager，避免大量线程。
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
        // 这里只保护查找 factory 的过程；拿到指针后释放 manager 锁。
        // 业务分发可能较慢，不能在持有 manager 锁时执行。
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
