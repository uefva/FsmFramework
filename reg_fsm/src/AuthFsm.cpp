//
// Created by MR on 2026/5/29.
//

#include "../inc/AuthFsm.h"

#include "../inc/FsmTableExecutor.h"

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
    unsigned int delayMs;
    const char* log;
    AuthAction action;
};
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

WS_TIMER_ID AuthFsm::StartNextTimer(const CMsg& currentMsg, MsgType nextType, unsigned int delayMs)
{
    CMsg nextMsg = currentMsg;
    nextMsg.type = nextType;
    nextMsg.fsmId = GetFsmId();

    return StartTimer(delayMs, nextMsg);
}

void AuthFsm::PrePrcMsg(CMsg& pBuf)
{
    Cfsm::PrePrcMsg(pBuf);
}

EerrNo AuthFsm::ProcessMsg(CMsg& pMsg)
{
    const AuthTransition AUTH_TRANSITIONS[] = {
        {IDLE, MSG_INIT, WORKING, true, MSG_CONNECT, 0, "[AUTH][MSG_INIT]: prepare auth context", &AuthFsm::HandleInit},
        {WORKING, MSG_CONNECT, WORKING, true, MSG_REQ, 0, "[AUTH][MSG_CONNECT]: connect auth service", &AuthFsm::HandleConnect},
        {WORKING, MSG_REQ, WORKING, true, MSG_RESP, 0, "[AUTH][MSG_REQ]: verify credentials", &AuthFsm::HandleReq},
        {WORKING, MSG_RESP, WORKING, true, MSG_CLOSE, 100, "[AUTH][MSG_RESP]: auth success", &AuthFsm::HandleResp},
        {WORKING, MSG_CLOSE, KILL_FSM, false, MSG_CLOSE, 0, "[AUTH][MSG_CLOSE]: close auth flow", &AuthFsm::HandleClose},
    };
    const unsigned int transitionCount =
        sizeof(AUTH_TRANSITIONS) / sizeof(AUTH_TRANSITIONS[0]);

    return ExecuteFsmTransition(
        *this,
        pMsg,
        AUTH_TRANSITIONS,
        transitionCount,
        "AuthFsm",
        [this](AuthAction action) {
            if (nullptr != action)
            {
                (this->*action)();
            }
        },
        [this](const AuthTransition& transition, const CMsg& currentMsg) {
            if (0 == transition.delayMs)
            {
                SendNextMsg(currentMsg, transition.nextEvent);
            }
            else
            {
                StartNextTimer(currentMsg, transition.nextEvent, transition.delayMs);
            }
        });
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
