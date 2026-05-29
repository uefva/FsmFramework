//
// Created by MR on 2026/5/29.
//

#include "../inc/AuthFsm.h"

#include <iostream>
#include <ostream>

namespace
{
using AuthAction = void (AuthFsm::*)();

// Authentication flow transition rule.
struct AuthTransition
{
    Tstate from;
    MsgType event;
    Tstate to;
    bool hasNext;
    MsgType nextEvent;
    const char* log;
    AuthAction action;
};

const AuthTransition* FindTransition(const AuthTransition* transitions,
                                     unsigned int transitionCount,
                                     Tstate state,
                                     MsgType event)
{
    for (unsigned int index = 0; index < transitionCount; ++index)
    {
        if ((transitions[index].from == state) &&
            (transitions[index].event == event))
        {
            return &transitions[index];
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
    const AuthTransition AUTH_TRANSITIONS[] = {
        {IDLE, MSG_INIT, WORKING, true, MSG_CONNECT, "[AUTH][MSG_INIT]: prepare auth context", &AuthFsm::HandleInit},
        {WORKING, MSG_CONNECT, WORKING, true, MSG_REQ, "[AUTH][MSG_CONNECT]: connect auth service", &AuthFsm::HandleConnect},
        {WORKING, MSG_REQ, WORKING, true, MSG_RESP, "[AUTH][MSG_REQ]: verify credentials", &AuthFsm::HandleReq},
        {WORKING, MSG_RESP, WORKING, true, MSG_CLOSE, "[AUTH][MSG_RESP]: auth success", &AuthFsm::HandleResp},
        {WORKING, MSG_CLOSE, KILL_FSM, false, MSG_CLOSE, "[AUTH][MSG_CLOSE]: close auth flow", &AuthFsm::HandleClose},
    };
    const unsigned int transitionCount =
        sizeof(AUTH_TRANSITIONS) / sizeof(AUTH_TRANSITIONS[0]);

    if (KILL_FSM == this->GetState())
    {
        return ERROR;
    }

    if (SUCCESS != Cfsm::ProcessMsg(pMsg))
    {
        std::cout << "AuthFsm::ProcessMsg error" << std::endl;
        return EerrNo::ERROR;
    }

    const AuthTransition* transition =
        FindTransition(AUTH_TRANSITIONS, transitionCount, this->GetState(), pMsg.type);
    if (nullptr == transition)
    {
        std::cout << "AuthFsm::ProcessMsg invalid transition, state="
                  << this->GetState() << " msg=" << pMsg.type << std::endl;
        return EerrNo::ERROR;
    }

    std::cout << transition->log << std::endl;
    if (nullptr != transition->action)
    {
        (this->*(transition->action))();
    }
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

void AuthFsm::HandleInit()
{
    std::cout << "[AUTH][MSG_INIT]: initialize authentication context" << std::endl;
}

void AuthFsm::HandleConnect()
{
    std::cout << "[AUTH][MSG_CONNECT]: prepare authentication connection" << std::endl;
}

void AuthFsm::HandleReq()
{
    std::cout << "[AUTH][MSG_REQ]: build authentication request" << std::endl;
}

void AuthFsm::HandleResp()
{
    std::cout << "[AUTH][MSG_RESP]: process authentication response" << std::endl;
}

void AuthFsm::HandleClose()
{
    std::cout << "[AUTH][MSG_CLOSE]: release authentication context" << std::endl;
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
