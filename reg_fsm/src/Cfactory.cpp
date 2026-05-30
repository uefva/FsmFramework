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

    for (auto& fsm : this->_fsm_list)
    {
        if (fsm)
        {
            fsm->Destroy();
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

    for (auto& fsm : this->_fsm_list)
    {
        if (fsm && (fsm->GetFsmId() == fsmId))
        {
            return fsm.get();
        }
    }

    return nullptr;
}

Cfsm* Cfactory::AddFsm()
{
    std::lock_guard<std::mutex> guard(this->_fsm_lock);

    if (this->_fsm_list.size() >= FSM_NUM_IN_FAC)
    {
        std::cout << "Cfactory::AddFsm failed, factory is full" << std::endl;
        return nullptr;
    }

    std::unique_ptr<Cfsm> fsm(CreateFsm());
    if (!fsm)
    {
        return nullptr;
    }

    fsm->SetFsmId(this->_nextFsmId++);
    fsm->SetFactory(this);
    fsm->Create();

    Cfsm* rawPtr = fsm.get();
    this->_fsm_list.push_back(std::move(fsm));

    std::cout << "Cfactory::AddFsm facId=" << this->_facId
              << " fsmId=" << rawPtr->GetFsmId() << std::endl;

    return rawPtr;
}

EerrNo Cfactory::FacMsgPrc(CMsg& msg)
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
            std::cout << "Cfactory::FacMsgPrc fsm not found, facId="
                      << this->_facId << " fsmId=" << msg.fsmId << std::endl;
            return INVALID_MSG;
        }

        fsm = AddFsm();
        if (nullptr == fsm)
        {
            return ERROR;
        }
    }

    return DispatchToFsm(fsm, msg);
}

EerrNo Cfactory::DispatchToFsm(Cfsm* fsm, CMsg& msg)
{
    if (nullptr == fsm)
    {
        return INVALID_MSG;
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

    auto it = std::find_if(
        this->_fsm_list.begin(),
        this->_fsm_list.end(),
        [fsmId](const std::unique_ptr<Cfsm>& fsm) {
            return fsm && (fsm->GetFsmId() == fsmId);
        });

    if (this->_fsm_list.end() == it)
    {
        return ERROR;
    }

    Cfsm* fsm = it->get();
    std::cout << "Cfactory::KillFsm facId=" << this->_facId
              << " fsmId=" << fsm->GetFsmId() << std::endl;

    fsm->Destroy();
    this->_fsm_list.erase(it);

    return SUCCESS;
}
