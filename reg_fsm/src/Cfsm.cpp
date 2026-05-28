//
// Created by MR on 2026/4/29.
//

#include <iostream>
#include <ostream>

#include "../inc/Cfactory.h"
#include "../inc/Cfactory_mgr.h"
#include "../inc/Cfsm.h"

void Cfsm::_changeState(Tstate state)
{
    SetState(state);
}

Cfsm::Cfsm(Tstate state)
    : _fsmId(0), _state(state), _prc(EerrNo::INIT), _factory(nullptr)
{
    // 构造函数只初始化基础字段，避免和 Cfactory::AddFsm 中的 Create 重复初始化。
}

Cfsm::~Cfsm()
{
}

Tstate Cfsm::GetState() const
{
    return this->_state;
}

void Cfsm::SetState(Tstate state)
{
    if (this->_state == state)
    {
        return;
    }

    Tstate oldState = this->_state;
    OnExitState(oldState, state);
    this->_state = state;
    OnEnterState(oldState, state);
}

unsigned int Cfsm::GetFsmId() const
{
    return this->_fsmId;
}

void Cfsm::SetFsmId(unsigned int fsmId)
{
    this->_fsmId = fsmId;
}

Cfactory* Cfsm::GetFactory()
{
    return this->_factory;
}

void Cfsm::SetFactory(Cfactory* factory)
{
    this->_factory = factory;
}

EerrNo Cfsm::SendMsg(const CMsg& msg)
{
    if ((nullptr == this->_factory) || (nullptr == this->_factory->GetFacMgr()))
    {
        return ERROR;
    }

    return this->_factory->GetFacMgr()->SendMsg(msg);
}

WS_TIMER_ID Cfsm::StartTimer(unsigned int timeoutMs, const CMsg& timeoutMsg)
{
    if ((nullptr == this->_factory) || (nullptr == this->_factory->GetFacMgr()))
    {
        return 0;
    }

    return this->_factory->GetFacMgr()->StartTimer(timeoutMs, timeoutMsg);
}

EerrNo Cfsm::StopTimer(WS_TIMER_ID timerId)
{
    if ((nullptr == this->_factory) || (nullptr == this->_factory->GetFacMgr()))
    {
        return ERROR;
    }

    return this->_factory->GetFacMgr()->StopTimer(timerId);
}

void Cfsm::OnExitState(Tstate oldState, Tstate newState)
{
}

void Cfsm::OnEnterState(Tstate oldState, Tstate newState)
{
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
    this->_prc = EerrNo::INIT;

    return SUCCESS;
}

EerrNo Cfsm::Destroy()
{
    std::cout << "Cfsm::Destroy" << std::endl;
    return SUCCESS;
}

EerrNo Cfsm::Destory()
{
    return Destroy();
}

void Cfsm::SaveMsg(const CMsg& msg)
{
    // 保存当前无法立即处理、但后续可恢复处理的消息。
    this->_save.push_back(msg);
}

void Cfsm::HoldMsg(const CMsg& msg)
{
    // 挂起消息，通常用于等待外部条件或状态变化后再处理。
    this->_hold.push_back(msg);
}

bool Cfsm::PopSavedMsg(CMsg& msg)
{
    if (this->_save.empty())
    {
        return false;
    }

    msg = this->_save.front();
    this->_save.pop_front();
    return true;
}

bool Cfsm::PopHeldMsg(CMsg& msg)
{
    if (this->_hold.empty())
    {
        return false;
    }

    msg = this->_hold.front();
    this->_hold.pop_front();
    return true;
}

void Cfsm::Print(bool detailFlag)
{
}
