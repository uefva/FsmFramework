//
// Created by MR on 2026/5/28.
//

#ifndef MYFSMDEMO_CFACTORY_H
#define MYFSMDEMO_CFACTORY_H

#include <memory>
#include <mutex>
#include <vector>

#include "Cfsm.h"

class Cfactory_mgr;

// Base factory for FSM instances.
// One Cfactory manages one type of Cfsm and owns its lifecycle.
class Cfactory
{
private:
    unsigned int _facId;        // Factory ID, matched with CMsg::serviceId.
    Cfactory_mgr* _facMgr;      // Owning manager.
    unsigned int _nextFsmId;    // Auto-increment FSM ID generator.

protected:
    std::vector<std::unique_ptr<Cfsm>> _fsm_list;   // FSM instances owned by this factory.
    mutable std::mutex _fsm_lock;   // Protects _fsm_list and _nextFsmId.

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

    // Implemented by concrete factories to create a business FSM.
    virtual Cfsm* CreateFsm() = 0;

    // Routes a message to the correct FSM. Default implementation handles
    // find/create/dispatch/reap; concrete factories only need to override CreateFsm().
    virtual EerrNo FacMsgPrc(CMsg& msg);
};

#endif //MYFSMDEMO_CFACTORY_H
