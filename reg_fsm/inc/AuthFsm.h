//
// Created by MR on 2026/5/29.
//

#ifndef MYFSMDEMO_AUTHFSM_H
#define MYFSMDEMO_AUTHFSM_H

#include "Cfsm.h"

template <typename FsmT, typename TransitionT>
EerrNo ExecuteFsmTransition(FsmT& fsm,
                            CMsg& msg,
                            const TransitionT* transitions,
                            unsigned int transitionCount,
                            const char* fsmName);

class AuthFsm : public Cfsm
{
private:
    using AuthAction = void (AuthFsm::*)();

    struct AuthTransition
    {
        Tstate from;
        MsgType event;
        Tstate to;
        bool hasNext;
        MsgType nextEvent;
        unsigned int delayMs;
        const char* log;
        AuthAction action;
    };

    template <typename FsmT, typename TransitionT>
    friend EerrNo ExecuteFsmTransition(FsmT& fsm,
                                       CMsg& msg,
                                       const TransitionT* transitions,
                                       unsigned int transitionCount,
                                       const char* fsmName);

    EerrNo SendNextMsg(const CMsg& currentMsg, MsgType nextType);
    WS_TIMER_ID StartNextTimer(const CMsg& currentMsg, MsgType nextType, unsigned int delayMs);
    static const AuthTransition* GetTransitions();
    static unsigned int GetTransitionCount();
    void RunAction(AuthAction action);
    EerrNo PostNextEvent(const AuthTransition& transition, const CMsg& currentMsg);
    void HandleInit();
    void HandleConnect();
    void HandleReq();
    void HandleResp();
    void HandleClose();

public:
    explicit AuthFsm();

    void PrePrcMsg(CMsg& pBuf) override;
    EerrNo ProcessMsg(CMsg& pMsg) override;
    void PostPrcMsg(CMsg& pBuf) override;

    EerrNo Destroy() override;
    EerrNo Destory() override;
    void Print(bool detailFlag) override;
};

#endif //MYFSMDEMO_AUTHFSM_H
