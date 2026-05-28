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

// 顶层状态机管理器：
// 1. 维护多个 Cfactory；
// 2. 提供线程安全消息泵；
// 3. 将外部消息按 serviceId 路由到对应工厂；
// 4. 提供简单定时器，把超时事件重新投递回消息泵。
class Cfactory_mgr
{
private:
    // 已注册的工厂列表，每个工厂管理一类 FSM。
    std::vector<Cfactory*> _fac_list;

    // 消息泵队列，所有外部事件、FSM 自投递事件、定时器事件都会进入这里。
    std::queue<CMsg> _pump;

    // 当前原型版定时器：每个 timer 对应一个控制块和一个线程。
    std::vector<std::shared_ptr<CTimerCtrl> > _timer_list;
    std::vector<std::thread> _timer_threads;
    std::thread _worker_thread;

    // 分别保护工厂列表、消息泵和定时器列表。
    std::mutex _fac_lock;
    std::mutex _pump_lock;
    std::mutex _timer_lock;

    // 阻塞式消息泵使用的条件变量。
    std::condition_variable _pump_cv;

    // Stop 后不再接受新消息，Run 在线程中会在队列清空后退出。
    bool _stopped;
    bool _running;

    WS_TIMER_ID _next_timer_id;

    Cfactory* FindFactory(unsigned int serviceId);
    EerrNo DispatchMsg(CMsg& msg);

public:
    Cfactory_mgr();
    ~Cfactory_mgr();

    // 注册一个工厂。manager 获得该 factory 的所有权，析构时负责 delete。
    EerrNo RegisterFactory(Cfactory* factory);

    // 线程安全投递消息，唤醒 Run 中阻塞等待的消息泵。
    EerrNo SendMsg(const CMsg& msg);

    // 非阻塞处理一条消息，适合测试或单线程驱动。
    EerrNo PumpOnce();

    // 非阻塞处理当前队列中的全部消息；不会等待后续定时器或新消息。
    void RunUntilEmpty();

    // 阻塞式消息循环，适合放到后台线程中运行。
    void Run();

    // 由 manager 自己启动后台消息泵线程，避免调用方分散管理 thread。
    EerrNo Start();

    // 请求消息泵停止；已在队列中的消息仍会被处理完。
    void Stop();

    // 等待后台消息泵线程退出。
    void Join();

    // 启动一个简单定时器，到期后投递 timeoutMsg。
    WS_TIMER_ID StartTimer(unsigned int timeoutMs, const CMsg& timeoutMsg);

    // 取消指定定时器。当前原型不会中断 sleep，只会阻止到期后投递消息。
    EerrNo StopTimer(WS_TIMER_ID timerId);

    void StopAllTimers();
};

#endif //MYFSMDEMO_CFACTORY_MGR_H
