//
// Created by MR on 2026/4/29.
//

#ifndef MYFSMDEMO_REG_FSM_H
#define MYFSMDEMO_REG_FSM_H

#include "common.h"

class Cfactory;

// Base finite state machine.
// Represents one FSM instance and stores state, owner factory, and deferred messages.
class Cfsm
{
private:
    unsigned int _fsmId;        // FSM instance ID inside its owner factory.
    Tstate _state;              // Current FSM state.
    EerrNo _prc;                // Last message processing result.
    Cfactory* _factory;         // Owner factory; used to access the manager.

protected:
    std::list<CMsg> _save;      // Saved messages that may be resumed later.
    std::list<CMsg> _hold;      // Held messages that are not processed yet.

    // Low-coupling helpers: business FSMs post events through the base class.
    EerrNo SendMsg(const CMsg& msg);
    WS_TIMER_ID StartTimer(unsigned int timeoutMs, const CMsg& timeoutMsg);
    EerrNo StopTimer(WS_TIMER_ID timerId);

    // State transition hooks that derived FSMs can override.
    virtual void OnExitState(Tstate from, Tstate to);
    virtual void OnEnterState(Tstate from, Tstate to);

public:
    void _changeState(Tstate state);            // Internal state change helper.

public:
    explicit Cfsm(Tstate state = IDLE);         // Construct an FSM, defaulting to IDLE.
    virtual ~Cfsm();                            // Resource cleanup is driven by the factory.

    unsigned int GetFsmId() const;
    void SetFsmId(unsigned int fsmId);

    Cfactory* GetFactory();
    void SetFactory(Cfactory* factory);

    Tstate GetState() const;                    // Get current state.
    void SetState(Tstate state);                // Set state and trigger enter/exit hooks.

    virtual void PrePrcMsg(CMsg& pBuf) = 0;     // Message pre-processing hook.
    virtual EerrNo ProcessMsg(CMsg& pMsg) = 0;  // Main message processing logic.
    virtual void PostPrcMsg(CMsg& pBuf) = 0;    // Message post-processing hook.

    virtual EerrNo Create();                    // FSM initialization.
    virtual EerrNo Destroy();                   // FSM cleanup.

    // Deferred-message queue helpers. Scheduling policy can be added later.
    void SaveMsg(const CMsg& msg);
    void HoldMsg(const CMsg& msg);
    bool PopSavedMsg(CMsg& msg);
    bool PopHeldMsg(CMsg& msg);

    virtual void Print(bool detailFlag);        // Print FSM information.
};


#endif //MYFSMDEMO_REG_FSM_H
