//
// TimerManager implementation.
// Ported from Cfactory_mgr; uses a callback instead of directly calling SendMsg.
//

#include "../inc/TimerManager.h"

#include "../inc/Logger.h"

#include <chrono>
#include <utility>

TimerManager::TimerManager(Callback callback)
    : _callback(std::move(callback)), _next_timer_id(1)
{
}

TimerManager::~TimerManager()
{
    StopAndJoin();
}

WS_TIMER_ID TimerManager::StartTimer(unsigned int timeoutMs, const CMsg& timeoutMsg)
{
    std::shared_ptr<CTimerCtrl> timerCtrl;

    {
        std::lock_guard<std::mutex> guard(this->_timer_lock);
        timerCtrl = std::make_shared<CTimerCtrl>(this->_next_timer_id++);
        this->_timer_list.push_back(timerCtrl);
    }

    LOG_DEBUG("TimerManager", "StartTimer timerId=" << timerCtrl->id
                              << " timeoutMs=" << timeoutMs
                              << " serviceId=" << timeoutMsg.serviceId
                              << " fsmId=" << timeoutMsg.fsmId
                              << " event=" << MsgTypeToString(timeoutMsg.type));

    std::thread timerThread([this, timeoutMs, timeoutMsg, timerCtrl]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(timeoutMs));
        if (timerCtrl->active.load())
        {
            LOG_DEBUG("TimerManager", "Timer expired timerId=" << timerCtrl->id
                                      << " event=" << MsgTypeToString(timeoutMsg.type));
            this->_callback(timeoutMsg);
        }
        else
        {
            LOG_DEBUG("TimerManager", "Timer canceled before expiration timerId="
                                      << timerCtrl->id);
        }
    });

    {
        std::lock_guard<std::mutex> guard(this->_timer_lock);
        this->_timer_threads.push_back(std::move(timerThread));
    }

    return timerCtrl->id;
}

EerrNo TimerManager::StopTimer(WS_TIMER_ID timerId)
{
    std::lock_guard<std::mutex> guard(this->_timer_lock);

    for (std::shared_ptr<CTimerCtrl>& timerCtrl : this->_timer_list)
    {
        if ((nullptr != timerCtrl) && (timerCtrl->id == timerId))
        {
            timerCtrl->active.store(false);
            LOG_DEBUG("TimerManager", "StopTimer timerId=" << timerId);
            return SUCCESS;
        }
    }

    LOG_WARN("TimerManager", "StopTimer failed, timerId=" << timerId);
    return TIMER_ERROR;
}

void TimerManager::StopAllTimers()
{
    std::lock_guard<std::mutex> guard(this->_timer_lock);

    for (std::shared_ptr<CTimerCtrl>& timerCtrl : this->_timer_list)
    {
        if (nullptr != timerCtrl)
        {
            timerCtrl->active.store(false);
        }
    }

    LOG_DEBUG("TimerManager", "StopAllTimers");
}

void TimerManager::StopAndJoin()
{
    StopAllTimers();

    std::vector<std::thread> timerThreads;
    {
        std::lock_guard<std::mutex> guard(this->_timer_lock);
        timerThreads.swap(this->_timer_threads);
    }

    for (std::thread& timerThread : timerThreads)
    {
        if (timerThread.joinable())
        {
            timerThread.join();
        }
    }

    {
        std::lock_guard<std::mutex> guard(this->_timer_lock);
        this->_timer_list.clear();
    }
}
