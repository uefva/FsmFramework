//
// Created by MR on 2026/5/28.
//

#include "../inc/Cfactory.h"

#include <algorithm>

#include "../inc/Cfactory_mgr.h"
#include "../inc/Logger.h"

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
        LOG_ERROR("Cfactory", "AddFsm failed, factory is full, facId="
                              << this->_facId);
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

    LOG_DEBUG("Cfactory", "AddFsm facId=" << this->_facId
                          << " fsmId=" << rawPtr->GetFsmId());

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
            LOG_WARN("Cfactory", "fsm not found, facId=" << this->_facId
                                 << " fsmId=" << msg.fsmId
                                 << " event=" << MsgTypeToString(msg.type));
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
        const unsigned int completedFsmId = fsm->GetFsmId();
        Cfactory_mgr* manager = GetFacMgr();
        if (nullptr != manager)
        {
            FsmCompletionEvent event;
            event.serviceId = this->_facId;
            event.fsmId = completedFsmId;
            event.sessionId = msg.sessionId;
            event.result = ret;
            event.finalEvent = msg.type;
            manager->NotifyFsmCompleted(event);
        }

        KillFsm(completedFsmId);
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
    LOG_DEBUG("Cfactory", "KillFsm facId=" << this->_facId
                          << " fsmId=" << fsm->GetFsmId());

    fsm->Destroy();
    this->_fsm_list.erase(it);

    return SUCCESS;
}
