//
// Created by MR on 2026/5/29.
//

#include "../inc/AuthFsm.h"

#include "../inc/FsmTableExecutor.h"
#include "../inc/Logger.h"

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

const AuthFsm::AuthTransition* AuthFsm::GetTransitions()
{
    static const AuthTransition transitions[] = {
        {IDLE, MSG_INIT, WORKING, true, MSG_CONNECT, 0, "[AUTH][MSG_INIT]: prepare auth context", &AuthFsm::HandleInit},
        {WORKING, MSG_CONNECT, WORKING, true, MSG_REQ, 0, "[AUTH][MSG_CONNECT]: connect auth service", &AuthFsm::HandleConnect},
        {WORKING, MSG_REQ, WORKING, true, MSG_RESP, 0, "[AUTH][MSG_REQ]: verify credentials", &AuthFsm::HandleReq},
        {WORKING, MSG_RESP, WORKING, true, MSG_CLOSE, 100, "[AUTH][MSG_RESP]: auth success", &AuthFsm::HandleResp},
        {WORKING, MSG_CLOSE, KILL_FSM, false, MSG_CLOSE, 0, "[AUTH][MSG_CLOSE]: close auth flow", &AuthFsm::HandleClose},
    };

    return transitions;
}

unsigned int AuthFsm::GetTransitionCount()
{
    static const unsigned int transitionCount = 5;
    return transitionCount;
}

void AuthFsm::RunAction(AuthAction action)
{
    if (nullptr != action)
    {
        (this->*action)();
    }
}

EerrNo AuthFsm::PostNextEvent(const AuthTransition& transition, const CMsg& currentMsg)
{
    if (0 == transition.delayMs)
    {
        return SendNextMsg(currentMsg, transition.nextEvent);
    }

    return (0 == StartNextTimer(currentMsg, transition.nextEvent, transition.delayMs))
        ? TIMER_ERROR
        : SUCCESS;
}

void AuthFsm::PrePrcMsg(CMsg& pBuf)
{
    Cfsm::PrePrcMsg(pBuf);
}

EerrNo AuthFsm::ProcessMsg(CMsg& pMsg)
{
    return ExecuteFsmTransition(*this, pMsg, GetTransitions(), GetTransitionCount(), "AuthFsm");
}

void AuthFsm::PostPrcMsg(CMsg& pBuf)
{
    Cfsm::PostPrcMsg(pBuf);
}

void AuthFsm::HandleInit()
{
    LOG_DEBUG("AuthFsm", "initialize authentication context");
}

void AuthFsm::HandleConnect()
{
    LOG_DEBUG("AuthFsm", "prepare authentication connection");
}

void AuthFsm::HandleReq()
{
    LOG_DEBUG("AuthFsm", "build authentication request");
}

void AuthFsm::HandleResp()
{
    LOG_DEBUG("AuthFsm", "process authentication response");
}

void AuthFsm::HandleClose()
{
    LOG_DEBUG("AuthFsm", "release authentication context");
}

EerrNo AuthFsm::Destroy()
{
    return Cfsm::Destroy();
}

void AuthFsm::Print(bool detailFlag)
{
    LOG_INFO("AuthFsm", "Print detailFlag=" << detailFlag
                        << " fsmId=" << GetFsmId()
                        << " state=" << StateToString(GetState()));
}
