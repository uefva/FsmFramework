//
// Created by MR on 2026/4/30.
//

#include "../inc/RegFsm.h"

#include "../inc/FsmTableExecutor.h"
#include "../inc/Logger.h"

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

const RegFsm::RegTransition* RegFsm::GetTransitions()
{
    static const RegTransition transitions[] = {
        {IDLE, MSG_INIT, WORKING, true, MSG_CONNECT, 0, "[REG][MSG_INIT]: start reg service", &RegFsm::HandleInit},
        {WORKING, MSG_CONNECT, WORKING, true, MSG_REQ, 0, "[REG][MSG_CONNECT]: connect reg service", &RegFsm::HandleConnect},
        {WORKING, MSG_REQ, WORKING, true, MSG_RESP, 0, "[REG][MSG_REQ]: request reg service", &RegFsm::HandleReq},
        {WORKING, MSG_RESP, WORKING, true, MSG_TIMEOUT, 100, "[REG][MSG_RESP]: response reg service", &RegFsm::HandleResp},
        {WORKING, MSG_TIMEOUT, WORKING, true, MSG_CLOSE, 0, "[REG][MSG_TIMEOUT]: timeout reg service", &RegFsm::HandleTimeout},
        {WORKING, MSG_CLOSE, KILL_FSM, false, MSG_CLOSE, 0, "[REG][MSG_CLOSE]: close reg service", &RegFsm::HandleClose},
    };

    return transitions;
}

unsigned int RegFsm::GetTransitionCount()
{
    static const unsigned int transitionCount = 6;
    return transitionCount;
}

void RegFsm::RunAction(RegAction action)
{
    if (nullptr != action)
    {
        (this->*action)();
    }
}

EerrNo RegFsm::PostNextEvent(const RegTransition& transition, const CMsg& currentMsg)
{
    if (0 == transition.delayMs)
    {
        return SendNextMsg(currentMsg, transition.nextEvent);
    }

    return (0 == StartNextTimer(currentMsg, transition.nextEvent, transition.delayMs))
        ? TIMER_ERROR
        : SUCCESS;
}

void RegFsm::PrePrcMsg(CMsg& pBuf)
{
    Cfsm::PrePrcMsg(pBuf);
}

EerrNo RegFsm::ProcessMsg(CMsg& pMsg)
{
    return ExecuteFsmTransition(*this, pMsg, GetTransitions(), GetTransitionCount(), "RegFsm");
}

void RegFsm::PostPrcMsg(CMsg& pBuf)
{
    Cfsm::PostPrcMsg(pBuf);
}

void RegFsm::HandleInit()
{
    LOG_DEBUG("RegFsm", "initialize registration context");
}

void RegFsm::HandleConnect()
{
    LOG_DEBUG("RegFsm", "prepare registration connection");
}

void RegFsm::HandleReq()
{
    LOG_DEBUG("RegFsm", "build registration request");
}

void RegFsm::HandleResp()
{
    LOG_DEBUG("RegFsm", "process registration response");
}

void RegFsm::HandleTimeout()
{
    LOG_DEBUG("RegFsm", "handle registration timeout");
}

void RegFsm::HandleClose()
{
    LOG_DEBUG("RegFsm", "release registration context");
}

EerrNo RegFsm::Destroy()
{
    return Cfsm::Destroy();
}

void RegFsm::Print(bool detailFlag)
{
    LOG_INFO("RegFsm", "Print detailFlag=" << detailFlag
                       << " fsmId=" << GetFsmId()
                       << " state=" << StateToString(GetState()));
}
