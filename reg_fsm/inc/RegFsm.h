//
// Created by MR on 2026/4/30.
//

#ifndef MYFSMDEMO_REGFSM_H
#define MYFSMDEMO_REGFSM_H
#include "Cfsm.h"

template <typename FsmT, typename TransitionT>
EerrNo ExecuteFsmTransition(FsmT& fsm,
                            CMsg& msg,
                            const TransitionT* transitions,
                            unsigned int transitionCount,
                            const char* fsmName);

class RegFsm : public Cfsm
{
private:
    using RegAction = void (RegFsm::*)();

    struct RegTransition
    {
        Tstate from;
        MsgType event;
        Tstate to;
        bool hasNext;
        MsgType nextEvent;
        unsigned int delayMs;
        const char* log;
        RegAction action;
    };

    template <typename FsmT, typename TransitionT>
    friend EerrNo ExecuteFsmTransition(FsmT& fsm,
                                       CMsg& msg,
                                       const TransitionT* transitions,
                                       unsigned int transitionCount,
                                       const char* fsmName);

    EerrNo SendNextMsg(const CMsg& currentMsg, MsgType nextType);
    WS_TIMER_ID StartNextTimer(const CMsg& currentMsg, MsgType nextType, unsigned int delayMs);
    static const RegTransition* GetTransitions();
    static unsigned int GetTransitionCount();
    void RunAction(RegAction action);
    EerrNo PostNextEvent(const RegTransition& transition, const CMsg& currentMsg);
    void HandleInit();
    void HandleConnect();
    void HandleReq();
    void HandleResp();
    void HandleTimeout();
    void HandleClose();

public:
    explicit RegFsm();

    void PrePrcMsg(CMsg& pBuf) override;
    EerrNo ProcessMsg(CMsg& pMsg) override;
    void PostPrcMsg(CMsg& pBuf) override;

    EerrNo Destroy() override;
    void Print(bool detailFlag) override;
};


#endif //MYFSMDEMO_REGFSM_H
