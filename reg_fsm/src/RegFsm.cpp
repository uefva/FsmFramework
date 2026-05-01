//
// Created by MR on 2026/4/30.
//

#include "../inc/RegFsm.h"

#include <iostream>
#include <ostream>

RegFsm::RegFsm() : Cfsm(IDLE)
{

}

void RegFsm::PrePrcMsg(CMsg& pBuf)
{
    Cfsm::PrePrcMsg(pBuf);
}

EerrNo RegFsm::ProcessMsg(CMsg& pMsg)
{
    if (KILL_FSM == this->GetState())
    {
        Destory();
    }

    if (SUCCESS != Cfsm::ProcessMsg(pMsg))
    {
        std::cout << "RegFsm::ProcessMsg error" << std::endl;
        return EerrNo::ERROR;
    };

    switch (pMsg.type)
    {
        case MSG_INIT:
            {
                std::cout << "[MSG_INIT]: start reg service";

                pMsg.type = MSG_CONNECT;
                this->SetState(WORKING);
                break;
            }
        case MSG_CONNECT:
            {
                std::cout << "[MSG_CONNECT]: start reg service";

                pMsg.type = MSG_REQ;
                break;
            }
        case MSG_REQ:
            {
                std::cout << "[MSG_REQ]: start reg service";

                pMsg.type = MSG_RESP;
                break;
            }
        case MSG_RESP:
            {
                std::cout << "[MSG_RESP]: start reg service";

                pMsg.type = MSG_TIMEOUT;
                break;
            }
        case MSG_TIMEOUT:
            {
                std::cout << "[MSG_TIMEOUT]: start reg service";

                pMsg.type = MSG_CLOSE;
                break;
            }
        case MSG_CLOSE:
            {
                std::cout << "[MSG_CLOSE]: start reg service";

                this->SetState(KILL_FSM);
                break;
            }
        default:
            return EerrNo::ERROR;
    }

    return SUCCESS;
}

void RegFsm::PostPrcMsg(CMsg& pBuf)
{
    Cfsm::PostPrcMsg(pBuf);
}

EerrNo RegFsm::Destory()
{
    return Cfsm::Destory();
}

void RegFsm::Print(bool detailFlag)
{
    std::cout << "[RegFsm::Print]" << std::endl;
}

