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

    // Prefer routing to an existing FSM when fsmId is provided.
    if (0 != msg.fsmId)
    {
        fsm = FindFsm(msg.fsmId);
    }

    if (nullptr == fsm)
    {
        // Only MSG_INIT is allowed to create a new registration FSM.
        if (MSG_INIT != msg.type)
        {
            std::cout << "RegFactory::FacMsgPrc fsm not found, fsmId="
                      << msg.fsmId << std::endl;
            return ERROR;
        }

        // The first initialization message creates a new FSM.
        fsm = AddFsm();
    }

    return DispatchToFsm(fsm, msg);
}
