//
// TimerManager extracted from Cfactory_mgr.
// Manages simple timer threads that post timeout messages via a callback.
//

#ifndef MYFSMDEMO_TIMERMANAGER_H
#define MYFSMDEMO_TIMERMANAGER_H

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "CMsg.h"
#include "common.h"

struct CTimerCtrl
{
    explicit CTimerCtrl(WS_TIMER_ID timerId) : id(timerId), active(true) {}
    WS_TIMER_ID id;
    std::atomic<bool> active;
};

class TimerManager
{
public:
    using Callback = std::function<EerrNo(const CMsg&)>;

    explicit TimerManager(Callback callback);
    ~TimerManager();

    TimerManager(const TimerManager&) = delete;
    TimerManager& operator=(const TimerManager&) = delete;

    WS_TIMER_ID StartTimer(unsigned int timeoutMs, const CMsg& timeoutMsg);
    EerrNo StopTimer(WS_TIMER_ID timerId);
    void StopAllTimers();
    void StopAndJoin();

private:
    Callback _callback;
    std::vector<std::shared_ptr<CTimerCtrl>> _timer_list;
    std::vector<std::thread> _timer_threads;
    std::mutex _timer_lock;
    WS_TIMER_ID _next_timer_id;
};

#endif //MYFSMDEMO_TIMERMANAGER_H
