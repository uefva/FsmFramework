//
// Created by MR on 2026/5/29.
//

#include "../inc/AuthFactory.h"

#include <iostream>

#include "../inc/AuthFsm.h"

AuthFactory::AuthFactory(unsigned int facId) : Cfactory(facId)
{
}

Cfsm* AuthFactory::CreateFsm()
{
    return new AuthFsm();
}

EerrNo AuthFactory::FacMsgPrc(CMsg& msg)
{
    Cfsm* fsm = nullptr;

    if (0 != msg.fsmId)
    {
        fsm = FindFsm(msg.fsmId);
    }

    if (nullptr == fsm)
    {
        if (MSG_INIT != msg.type)
        {
            std::cout << "AuthFactory::FacMsgPrc fsm not found, fsmId="
                      << msg.fsmId << std::endl;
            return ERROR;
        }

        fsm = AddFsm();
    }

    return DispatchToFsm(fsm, msg);
}
