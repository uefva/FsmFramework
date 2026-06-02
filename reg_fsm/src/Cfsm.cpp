//
// Created by MR on 2026/4/29.
//

#include "../inc/Cfactory.h"
#include "../inc/Cfactory_mgr.h"
#include "../inc/Cfsm.h"
#include "../inc/Logger.h"

void Cfsm::_changeState(Tstate state)
{
    SetState(state);
}

Cfsm::Cfsm(Tstate state)
    : _fsmId(0), _state(state), _prc(EerrNo::INIT), _factory(nullptr)
{
    // Only initialize basic fields here to avoid duplicating Create in Cfactory::AddFsm.
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
        return INVALID_STATE;
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
        return TIMER_ERROR;
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
    LOG_DEBUG("Cfsm", "PrePrcMsg fsmId=" << this->_fsmId
                       << " state=" << StateToString(this->_state)
                       << " event=" << MsgTypeToString(pBuf.type));
}

EerrNo Cfsm::ProcessMsg(CMsg& pMsg)
{
    LOG_DEBUG("Cfsm", "ProcessMsg fsmId=" << this->_fsmId
                       << " state=" << StateToString(this->_state)
                       << " event=" << MsgTypeToString(pMsg.type));

    return EerrNo::SUCCESS;
}

void Cfsm::PostPrcMsg(CMsg& pBuf)
{
    LOG_DEBUG("Cfsm", "PostPrcMsg fsmId=" << this->_fsmId
                       << " state=" << StateToString(this->_state)
                       << " event=" << MsgTypeToString(pBuf.type));
}

EerrNo Cfsm::Create()
{
    this->_prc = EerrNo::INIT;

    return SUCCESS;
}

EerrNo Cfsm::Destroy()
{
    LOG_DEBUG("Cfsm", "Destroy fsmId=" << this->_fsmId
                      << " state=" << StateToString(this->_state));
    return SUCCESS;
}

void Cfsm::SaveMsg(const CMsg& msg)
{
    // Save a message that cannot be processed immediately but may resume later.
    this->_save.push_back(msg);
}

void Cfsm::HoldMsg(const CMsg& msg)
{
    // Hold a message while waiting for an external condition or state change.
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
