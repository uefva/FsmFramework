//
// Created by MR on 2026/4/29.
//

#ifndef MYFSMDEMO_REG_FSM_H
#define MYFSMDEMO_REG_FSM_H

#include "common.h"

class Cfactory;

// 有限状态机基类：
// 代表一个具体 FSM 实例，保存状态、所属工厂和消息暂存队列。
class Cfsm
{
private:
    unsigned int _fsmId;        // FSM 在所属工厂中的实例 ID。
    Tstate _state;              // FSM 当前状态。
    EerrNo _prc;                // 最近一次消息处理结果。
    Cfactory* _factory;         // 所属工厂，FSM 可通过它访问 manager。

protected:
    std::list<CMsg> _save;      // 暂存消息：后续条件满足时可继续处理。
    std::list<CMsg> _hold;      // 挂起消息：当前不处理，但也不丢弃。

    // 低耦合辅助接口：业务 FSM 不直接依赖 Cfactory_mgr，只通过基类投递事件。
    EerrNo SendMsg(const CMsg& msg);
    WS_TIMER_ID StartTimer(unsigned int timeoutMs, const CMsg& timeoutMsg);
    EerrNo StopTimer(WS_TIMER_ID timerId);

    // 状态切换钩子，派生类可按需重写。
    virtual void OnExitState(Tstate oldState, Tstate newState);
    virtual void OnEnterState(Tstate oldState, Tstate newState);

public:
    void _changeState(Tstate state);            // 内部状态切换接口。

public:
    explicit Cfsm(Tstate state = IDLE);         // 构造 FSM，默认从 IDLE 开始。
    virtual ~Cfsm();                            // 析构函数，资源回收由 factory 主导。

    unsigned int GetFsmId() const;
    void SetFsmId(unsigned int fsmId);

    Cfactory* GetFactory();
    void SetFactory(Cfactory* factory);

    Tstate GetState() const;                    // 获取当前状态。
    void SetState(Tstate state);                // 设置当前状态，并触发进入/退出钩子。

    virtual void PrePrcMsg(CMsg& pBuf) = 0;     // 消息前处理钩子。
    virtual EerrNo ProcessMsg(CMsg& pMsg) = 0;  // 消息主处理逻辑，由业务 FSM 实现。
    virtual void PostPrcMsg(CMsg& pBuf) = 0;    // 消息后处理钩子。

    virtual EerrNo Create();                    // FSM 创建初始化。
    virtual EerrNo Destroy();                   // FSM 销毁清理。
    virtual EerrNo Destory();                   // 兼容旧拼写，内部转调 Destroy。

    // 消息暂存队列接口，当前先提供基础能力，后续可接入调度策略。
    void SaveMsg(const CMsg& msg);
    void HoldMsg(const CMsg& msg);
    bool PopSavedMsg(CMsg& msg);
    bool PopHeldMsg(CMsg& msg);

    virtual void Print(bool detailFlag);        // 打印 FSM 信息。
};

#endif //MYFSMDEMO_REG_FSM_H
