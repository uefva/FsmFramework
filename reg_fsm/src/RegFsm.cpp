//
// Created by MR on 2026/4/30.
//

#include "../inc/RegFsm.h"

#include <iostream>
#include <ostream>

namespace
{
// 注册流程转移规则：
// from + event 命中后，切换到 to，并按配置投递 nextEvent。
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
    // 当前使用线性查找，规则较少时足够直观；后续可换成 map 加速。
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
    // 复制当前消息的路由字段，只替换事件类型和 FSM ID。
    CMsg nextMsg = currentMsg;
    nextMsg.type = nextType;
    nextMsg.fsmId = GetFsmId();

    return SendMsg(nextMsg);
}

WS_TIMER_ID RegFsm::StartNextTimer(const CMsg& currentMsg, MsgType nextType, unsigned int delayMs)
{
    // 定时器到期后投递的仍然是普通消息，因此后续分发链路保持一致。
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
    // 终止态 FSM 不再处理消息，销毁动作由 factory 统一负责。
    if (KILL_FSM == this->GetState())
    {
        return ERROR;
    }

    if (SUCCESS != Cfsm::ProcessMsg(pMsg))
    {
        std::cout << "RegFsm::ProcessMsg error" << std::endl;
        return EerrNo::ERROR;
    };

    // 用“当前状态 + 当前事件”查表，避免单纯按消息类型误处理非法状态。
    const RegTransition* transition = FindTransition(this->GetState(), pMsg.type);
    if (nullptr == transition)
    {
        std::cout << "RegFsm::ProcessMsg invalid transition, state="
                  << this->GetState() << " msg=" << pMsg.type << std::endl;
        return EerrNo::ERROR;
    }

    // 命中转移后先切换状态，再投递后续事件。
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

