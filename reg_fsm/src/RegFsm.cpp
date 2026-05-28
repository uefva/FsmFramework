//
// Created by MR on 2026/4/30.
//

#include "../inc/RegFsm.h"

#include <iostream>
#include <ostream>

namespace
{
// Registration flow transition rule:
// when from + event matches, switch to to and optionally post nextEvent.
struct RegTransition
{
    Tstate from;
    MsgType event;
    Tstate to;
    bool hasNext;
    MsgType nextEvent;
    unsigned int delayMs;
    const char* log;
};

const RegTransition REG_TRANSITIONS[] = {
    {IDLE, MSG_INIT, WORKING, true, MSG_CONNECT, 0, "[MSG_INIT]: start reg service"},
    {WORKING, MSG_CONNECT, WORKING, true, MSG_REQ, 0, "[MSG_CONNECT]: connect reg service"},
    {WORKING, MSG_REQ, WORKING, true, MSG_RESP, 0, "[MSG_REQ]: request reg service"},
    {WORKING, MSG_RESP, WORKING, true, MSG_TIMEOUT, 10, "[MSG_RESP]: response reg service"},
    {WORKING, MSG_TIMEOUT, WORKING, true, MSG_CLOSE, 0, "[MSG_TIMEOUT]: timeout reg service"},
    {WORKING, MSG_CLOSE, KILL_FSM, false, MSG_CLOSE, 0, "[MSG_CLOSE]: close reg service"},
};

const RegTransition* FindTransition(Tstate state, MsgType event)
{
    // Linear lookup is enough while the transition table is small.
    const unsigned int transitionCount =
        sizeof(REG_TRANSITIONS) / sizeof(REG_TRANSITIONS[0]);

    for (unsigned int index = 0; index < transitionCount; ++index)
    {
        if ((REG_TRANSITIONS[index].from == state) &&
            (REG_TRANSITIONS[index].event == event))
        {
            return &REG_TRANSITIONS[index];
        }
    }

    return nullptr;
}
}

RegFsm::RegFsm() : Cfsm(IDLE)
{

}

EerrNo RegFsm::SendNextMsg(const CMsg& currentMsg, MsgType nextType)
{
    // Preserve routing fields and only replace the event type and FSM ID.
    CMsg nextMsg = currentMsg;
    nextMsg.type = nextType;
    nextMsg.fsmId = GetFsmId();

    return SendMsg(nextMsg);
}

WS_TIMER_ID RegFsm::StartNextTimer(const CMsg& currentMsg, MsgType nextType, unsigned int delayMs)
{
    // Timer expiration posts a normal message, so dispatch remains consistent.
    CMsg nextMsg = currentMsg;
    nextMsg.type = nextType;
    nextMsg.fsmId = GetFsmId();

    return StartTimer(delayMs, nextMsg);
}

void RegFsm::PrePrcMsg(CMsg& pBuf)
{
    Cfsm::PrePrcMsg(pBuf);
}

EerrNo RegFsm::ProcessMsg(CMsg& pMsg)
{
    // Terminal FSMs no longer process messages; the factory owns destruction.
    if (KILL_FSM == this->GetState())
    {
        return ERROR;
    }

    if (SUCCESS != Cfsm::ProcessMsg(pMsg))
    {
        std::cout << "RegFsm::ProcessMsg error" << std::endl;
        return EerrNo::ERROR;
    };

    // Use current state + current event to avoid accepting invalid transitions.
    const RegTransition* transition = FindTransition(this->GetState(), pMsg.type);
    if (nullptr == transition)
    {
        std::cout << "RegFsm::ProcessMsg invalid transition, state="
                  << this->GetState() << " msg=" << pMsg.type << std::endl;
        return EerrNo::ERROR;
    }

    // Apply the transition first, then post the next event if configured.
    std::cout << transition->log << std::endl;
    this->SetState(transition->to);

    if (transition->hasNext)
    {
        if (0 == transition->delayMs)
        {
            SendNextMsg(pMsg, transition->nextEvent);
        }
        else
        {
            StartNextTimer(pMsg, transition->nextEvent, transition->delayMs);
        }
    }

    return SUCCESS;
}

void RegFsm::PostPrcMsg(CMsg& pBuf)
{
    Cfsm::PostPrcMsg(pBuf);
}

EerrNo RegFsm::Destroy()
{
    return Cfsm::Destroy();
}

EerrNo RegFsm::Destory()
{
    return Destroy();
}

void RegFsm::Print(bool detailFlag)
{
    std::cout << "[RegFsm::Print]" << std::endl;
}
