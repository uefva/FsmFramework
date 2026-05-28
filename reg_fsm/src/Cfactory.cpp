//
// Created by MR on 2026/5/28.
//

#include "../inc/Cfactory.h"

#include <algorithm>
#include <iostream>

Cfactory::Cfactory(unsigned int facId)
    : _facId(facId), _facMgr(nullptr), _nextFsmId(1)
{
    this->_fsm_list.reserve(FSM_NUM_IN_FAC);
}

Cfactory::~Cfactory()
{
    std::lock_guard<std::mutex> guard(this->_fsm_lock);

    // The factory owns every FSM it creates and releases them on destruction.
    for (Cfsm* fsm : this->_fsm_list)
    {
        if (nullptr != fsm)
        {
            fsm->Destroy();
            delete fsm;
        }
    }

    this->_fsm_list.clear();
}

unsigned int Cfactory::GetFacId() const
{
    return this->_facId;
}

Cfactory_mgr* Cfactory::GetFacMgr()
{
    return this->_facMgr;
}

void Cfactory::SetFacMgr(Cfactory_mgr* facMgr)
{
    this->_facMgr = facMgr;
}

Cfsm* Cfactory::FindFsm(unsigned int fsmId)
{
    std::lock_guard<std::mutex> guard(this->_fsm_lock);

    for (Cfsm* fsm : this->_fsm_list)
    {
        if ((nullptr != fsm) && (fsm->GetFsmId() == fsmId))
        {
            return fsm;
        }
    }

    return nullptr;
}

Cfsm* Cfactory::AddFsm()
{
    std::lock_guard<std::mutex> guard(this->_fsm_lock);

    // Reject new FSM creation when the factory reaches its capacity limit.
    if (this->_fsm_list.size() >= FSM_NUM_IN_FAC)
    {
        std::cout << "Cfactory::AddFsm failed, factory is full" << std::endl;
        return nullptr;
    }

    Cfsm* fsm = CreateFsm();
    if (nullptr == fsm)
    {
        return nullptr;
    }

    // Assign an instance ID and link the FSM back to its owner factory.
    fsm->SetFsmId(this->_nextFsmId++);
    fsm->SetFactory(this);
    fsm->Create();
    this->_fsm_list.push_back(fsm);

    std::cout << "Cfactory::AddFsm facId=" << this->_facId
              << " fsmId=" << fsm->GetFsmId() << std::endl;

    return fsm;
}

EerrNo Cfactory::DispatchToFsm(Cfsm* fsm, CMsg& msg)
{
    if (nullptr == fsm)
    {
        return ERROR;
    }

    // Keep later self-posted messages routed to the same FSM.
    msg.fsmId = fsm->GetFsmId();

    // Do not hold _fsm_lock while running business logic.
    fsm->PrePrcMsg(msg);
    EerrNo ret = fsm->ProcessMsg(msg);
    fsm->PostPrcMsg(msg);

    // A terminal FSM is reclaimed by the factory instead of deleting itself.
    if (KILL_FSM == fsm->GetState())
    {
        KillFsm(fsm->GetFsmId());
    }

    return ret;
}

EerrNo Cfactory::KillFsm(unsigned int fsmId)
{
    std::lock_guard<std::mutex> guard(this->_fsm_lock);

    // Find the instance by fsmId, destroy it, release memory, and remove it.
    auto it = std::find_if(
        this->_fsm_list.begin(),
        this->_fsm_list.end(),
        [fsmId](Cfsm* fsm) {
            return (nullptr != fsm) && (fsm->GetFsmId() == fsmId);
        });

    if (this->_fsm_list.end() == it)
    {
        return ERROR;
    }

    Cfsm* fsm = *it;
    std::cout << "Cfactory::KillFsm facId=" << this->_facId
              << " fsmId=" << fsm->GetFsmId() << std::endl;

    fsm->Destroy();
    delete fsm;
    this->_fsm_list.erase(it);

    return SUCCESS;
}
