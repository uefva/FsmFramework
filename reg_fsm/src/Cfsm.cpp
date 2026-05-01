//
// Created by MR on 2026/4/29.
//

#include <iostream>
#include <ostream>

#include "../inc/CFsm.h"


void Cfsm::_changeState(Tstate state)
{
    this->_state = state;
}

Cfsm::Cfsm(Tstate state)
{
    Cfsm::Create();
}

Cfsm::~Cfsm()
{
    Cfsm::Destory();
}

Tstate Cfsm::GetState() const
{
    return this->_state;
}

void Cfsm::SetState(Tstate state)
{
    this->_state = state;
}

void Cfsm::PrePrcMsg(CMsg& pBuf)
{
    std::cout << "Cfsm::PrePrcMsg" << std::endl;
}

EerrNo Cfsm::ProcessMsg(CMsg& pMsg)
{
    std::cout << "Cfsm::ProcessMsg" << std::endl;

    return EerrNo::SUCCESS;
}

void Cfsm::PostPrcMsg(CMsg& pBuf)
{
    std::cout << "Cfsm::PostPrcMsg" << std::endl;
}


EerrNo Cfsm::Create()
{
    this->_state = IDLE;
    this->_prc = EerrNo::INIT;

    return SUCCESS;
}

EerrNo Cfsm::Destory()
{
    std::cout << "Cfsm::Destory" << std::endl;
    return SUCCESS;
}

void Cfsm::Print(bool detailFlag)
{
}
