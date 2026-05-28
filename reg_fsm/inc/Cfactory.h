//
// Created by MR on 2026/5/28.
//

#ifndef MYFSMDEMO_CFACTORY_H
#define MYFSMDEMO_CFACTORY_H

#include <mutex>
#include <vector>

#include "Cfsm.h"

class Cfactory_mgr;

// 状态机工厂基类：
// 一个 Cfactory 管理同一类 Cfsm 实例，负责实例创建、消息路由和生命周期回收。
class Cfactory
{
private:
    unsigned int _facId;        // 工厂 ID，对应 CMsg::serviceId。
    Cfactory_mgr* _facMgr;      // 所属 manager。
    unsigned int _nextFsmId;    // 当前工厂内部分配 FSM ID 的自增计数器。

protected:
    std::vector<Cfsm*> _fsm_list;   // 本工厂管理的 FSM 实例列表。
    mutable std::mutex _fsm_lock;   // 保护 _fsm_list 和 _nextFsmId。

    Cfsm* FindFsm(unsigned int fsmId);
    Cfsm* AddFsm();
    EerrNo DispatchToFsm(Cfsm* fsm, CMsg& msg);
    EerrNo KillFsm(unsigned int fsmId);

public:
    explicit Cfactory(unsigned int facId);
    virtual ~Cfactory();

    unsigned int GetFacId() const;
    Cfactory_mgr* GetFacMgr();
    void SetFacMgr(Cfactory_mgr* facMgr);

    // 由具体工厂实现，决定创建哪种业务 FSM。
    virtual Cfsm* CreateFsm() = 0;

    // 由具体工厂实现，决定消息如何路由到具体 FSM。
    virtual EerrNo FacMsgPrc(CMsg& msg) = 0;
};

#endif //MYFSMDEMO_CFACTORY_H
