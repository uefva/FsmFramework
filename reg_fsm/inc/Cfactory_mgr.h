//
// Created by MR on 2026/5/28.
//

#ifndef MYFSMDEMO_CFACTORY_MGR_H
#define MYFSMDEMO_CFACTORY_MGR_H

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "Cfactory.h"

struct CTimerCtrl
{
    explicit CTimerCtrl(WS_TIMER_ID timerId) : id(timerId), active(true)
    {
    }

    WS_TIMER_ID id;
    std::atomic<bool> active;
};

// Top-level FSM manager:
// 1. Owns multiple Cfactory instances.
// 2. Provides a thread-safe message pump.
// 3. Routes external messages to factories by serviceId.
// 4. Provides simple timers that post timeout events back to the message pump.
class Cfactory_mgr
{
private:
    // Registered factories. Each factory manages one type of FSM.
    std::vector<Cfactory*> _fac_list;

    // Message pump queue for external events, FSM-generated events, and timer events.
    std::queue<CMsg> _pump;

    // Prototype timer implementation: one control block and one thread per timer.
    std::vector<std::shared_ptr<CTimerCtrl> > _timer_list;
    std::vector<std::thread> _timer_threads;
    std::thread _worker_thread;

    // Locks for the factory list, message pump, and timer list.
    std::mutex _fac_lock;
    std::mutex _pump_lock;
    std::mutex _timer_lock;

    // Condition variable used by the blocking message pump.
    std::condition_variable _pump_cv;

    // After Stop, new messages are rejected and Run exits after the queue is drained.
    bool _stopped;
    bool _running;

    WS_TIMER_ID _next_timer_id;

    Cfactory* FindFactory(unsigned int serviceId);
    EerrNo DispatchMsg(CMsg& msg);

public:
    Cfactory_mgr();
    ~Cfactory_mgr();

    // Register a factory. The manager takes ownership and deletes it on destruction.
    EerrNo RegisterFactory(Cfactory* factory);

    // Thread-safe message posting; wakes the blocking message pump.
    EerrNo SendMsg(const CMsg& msg);

    // Process one message without blocking. Useful for tests or single-threaded driving.
    EerrNo PumpOnce();

    // Drain the current queue without waiting for future messages or timers.
    void RunUntilEmpty();

    // Blocking message loop, normally run by the manager-owned worker thread.
    void Run();

    // Start the manager-owned background message-pump thread.
    EerrNo Start();

    // Request the message pump to stop; queued messages are still processed.
    void Stop();

    // Wait for the background message-pump thread to exit.
    void Join();

    // Start a simple timer that posts timeoutMsg when it expires.
    WS_TIMER_ID StartTimer(unsigned int timeoutMs, const CMsg& timeoutMsg);

    // Cancel a timer. This prototype does not interrupt sleep; it only prevents posting.
    EerrNo StopTimer(WS_TIMER_ID timerId);

    void StopAllTimers();
};

#endif //MYFSMDEMO_CFACTORY_MGR_H
