//
// Created by MR on 2026/5/28.
//

#include "../inc/RegFactory.h"

#include <iostream>

#include "../inc/RegFsm.h"

RegFactory::RegFactory(unsigned int facId) : Cfactory(facId)
{
}

Cfsm* RegFactory::CreateFsm()
{
    return new RegFsm();
}

EerrNo RegFactory::FacMsgPrc(CMsg& msg)
{
    Cfsm* fsm = nullptr;

    // fsmId 非 0 时，优先路由到已有 FSM。
    if (0 != msg.fsmId)
    {
        fsm = FindFsm(msg.fsmId);
    }

    if (nullptr == fsm)
    {
        // 当前约定：只有 MSG_INIT 可以创建新的注册 FSM。
        if (MSG_INIT != msg.type)
        {
            std::cout << "RegFactory::FacMsgPrc fsm not found, fsmId="
                      << msg.fsmId << std::endl;
            return ERROR;
        }

        // 首条初始化消息创建新 FSM，后续消息必须携带 fsmId。
        fsm = AddFsm();
    }

    return DispatchToFsm(fsm, msg);
}
