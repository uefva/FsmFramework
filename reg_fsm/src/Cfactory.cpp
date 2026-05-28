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

    // 工厂拥有自己创建的 FSM，析构时统一销毁。
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

    // 当前工厂达到容量上限时拒绝创建新 FSM。
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

    // 工厂负责给 FSM 分配实例 ID，并建立反向 owner 指针。
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

    // 确保后续自投递消息能路由回同一个 FSM。
    msg.fsmId = fsm->GetFsmId();

    // 不在持有 _fsm_lock 时调用业务处理，避免业务回调 SendMsg/StartTimer 时扩大锁范围。
    fsm->PrePrcMsg(msg);
    EerrNo ret = fsm->ProcessMsg(msg);
    fsm->PostPrcMsg(msg);

    // FSM 进入终止态后，由工厂集中回收，避免业务 FSM 自己 delete 自己。
    if (KILL_FSM == fsm->GetState())
    {
        KillFsm(fsm->GetFsmId());
    }

    return ret;
}

EerrNo Cfactory::KillFsm(unsigned int fsmId)
{
    std::lock_guard<std::mutex> guard(this->_fsm_lock);

    // 根据 fsmId 找到实例，调用 Destroy 后释放内存并从列表移除。
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
