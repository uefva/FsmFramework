//
// Created by MR on 2026/4/30.
//

#include "../inc/RegFsm.h"

#include "../inc/FsmTableExecutor.h"

#include <iostream>
#include <ostream>

namespace
{
using RegAction = void (RegFsm::*)();

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
    RegAction action;
};
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
    const RegTransition REG_TRANSITIONS[] = {
        {IDLE, MSG_INIT, WORKING, true, MSG_CONNECT, 0, "[REG][MSG_INIT]: start reg service", &RegFsm::HandleInit},
        {WORKING, MSG_CONNECT, WORKING, true, MSG_REQ, 0, "[REG][MSG_CONNECT]: connect reg service", &RegFsm::HandleConnect},
        {WORKING, MSG_REQ, WORKING, true, MSG_RESP, 0, "[REG][MSG_REQ]: request reg service", &RegFsm::HandleReq},
        {WORKING, MSG_RESP, WORKING, true, MSG_TIMEOUT, 10, "[REG][MSG_RESP]: response reg service", &RegFsm::HandleResp},
        {WORKING, MSG_TIMEOUT, WORKING, true, MSG_CLOSE, 0, "[REG][MSG_TIMEOUT]: timeout reg service", &RegFsm::HandleTimeout},
        {WORKING, MSG_CLOSE, KILL_FSM, false, MSG_CLOSE, 0, "[REG][MSG_CLOSE]: close reg service", &RegFsm::HandleClose},
    };
    const unsigned int transitionCount =
        sizeof(REG_TRANSITIONS) / sizeof(REG_TRANSITIONS[0]);

    return ExecuteFsmTransition(
        *this,
        pMsg,
        REG_TRANSITIONS,
        transitionCount,
        "RegFsm",
        [this](RegAction action) {
            if (nullptr != action)
            {
                (this->*action)();
            }
        },
        [this](const RegTransition& transition, const CMsg& currentMsg) {
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

void RegFsm::PostPrcMsg(CMsg& pBuf)
{
    Cfsm::PostPrcMsg(pBuf);
}

void RegFsm::HandleInit()
{
    std::cout << "[REG][MSG_INIT]: initialize registration context" << std::endl;
}

void RegFsm::HandleConnect()
{
    std::cout << "[REG][MSG_CONNECT]: prepare registration connection" << std::endl;
}

void RegFsm::HandleReq()
{
    std::cout << "[REG][MSG_REQ]: build registration request" << std::endl;
}

void RegFsm::HandleResp()
{
    std::cout << "[REG][MSG_RESP]: process registration response" << std::endl;
}

void RegFsm::HandleTimeout()
{
    std::cout << "[REG][MSG_TIMEOUT]: handle registration timeout" << std::endl;
}

void RegFsm::HandleClose()
{
    std::cout << "[REG][MSG_CLOSE]: release registration context" << std::endl;
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
