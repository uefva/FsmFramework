//
// Created by MR on 2026/5/29.
//

#include "../inc/AuthFsm.h"

#include <iostream>
#include <ostream>

namespace
{
// Authentication flow transition rule.
struct AuthTransition
{
    Tstate from;
    MsgType event;
    Tstate to;
    bool hasNext;
    MsgType nextEvent;
    const char* log;
};

const AuthTransition AUTH_TRANSITIONS[] = {
    {IDLE, MSG_INIT, WORKING, true, MSG_CONNECT, "[AUTH][MSG_INIT]: prepare auth context"},
    {WORKING, MSG_CONNECT, WORKING, true, MSG_REQ, "[AUTH][MSG_CONNECT]: connect auth service"},
    {WORKING, MSG_REQ, WORKING, true, MSG_RESP, "[AUTH][MSG_REQ]: verify credentials"},
    {WORKING, MSG_RESP, WORKING, true, MSG_CLOSE, "[AUTH][MSG_RESP]: auth success"},
    {WORKING, MSG_CLOSE, KILL_FSM, false, MSG_CLOSE, "[AUTH][MSG_CLOSE]: close auth flow"},
};

const AuthTransition* FindTransition(Tstate state, MsgType event)
{
    const unsigned int transitionCount =
        sizeof(AUTH_TRANSITIONS) / sizeof(AUTH_TRANSITIONS[0]);

    for (unsigned int index = 0; index < transitionCount; ++index)
    {
        if ((AUTH_TRANSITIONS[index].from == state) &&
            (AUTH_TRANSITIONS[index].event == event))
        {
            return &AUTH_TRANSITIONS[index];
        }
    }

    return nullptr;
}
}

AuthFsm::AuthFsm() : Cfsm(IDLE)
{
}

EerrNo AuthFsm::SendNextMsg(const CMsg& currentMsg, MsgType nextType)
{
    CMsg nextMsg = currentMsg;
    nextMsg.type = nextType;
    nextMsg.fsmId = GetFsmId();

    return SendMsg(nextMsg);
}

void AuthFsm::PrePrcMsg(CMsg& pBuf)
{
    Cfsm::PrePrcMsg(pBuf);
}

EerrNo AuthFsm::ProcessMsg(CMsg& pMsg)
{
    if (KILL_FSM == this->GetState())
    {
        return ERROR;
    }

    if (SUCCESS != Cfsm::ProcessMsg(pMsg))
    {
        std::cout << "AuthFsm::ProcessMsg error" << std::endl;
        return EerrNo::ERROR;
    }

    const AuthTransition* transition = FindTransition(this->GetState(), pMsg.type);
    if (nullptr == transition)
    {
        std::cout << "AuthFsm::ProcessMsg invalid transition, state="
                  << this->GetState() << " msg=" << pMsg.type << std::endl;
        return EerrNo::ERROR;
    }

    std::cout << transition->log << std::endl;
    this->SetState(transition->to);

    if (transition->hasNext)
    {
        SendNextMsg(pMsg, transition->nextEvent);
    }

    return SUCCESS;
}

void AuthFsm::PostPrcMsg(CMsg& pBuf)
{
    Cfsm::PostPrcMsg(pBuf);
}

EerrNo AuthFsm::Destroy()
{
    return Cfsm::Destroy();
}

EerrNo AuthFsm::Destory()
{
    return Destroy();
}

void AuthFsm::Print(bool detailFlag)
{
    std::cout << "[AuthFsm::Print]" << std::endl;
}
