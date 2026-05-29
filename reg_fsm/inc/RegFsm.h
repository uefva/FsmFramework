//
// Created by MR on 2026/4/30.
//

#ifndef MYFSMDEMO_REGFSM_H
#define MYFSMDEMO_REGFSM_H
#include "Cfsm.h"


class RegFsm : public Cfsm
{
private:
    EerrNo SendNextMsg(const CMsg& currentMsg, MsgType nextType);
    WS_TIMER_ID StartNextTimer(const CMsg& currentMsg, MsgType nextType, unsigned int delayMs);
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
    EerrNo Destory() override;
    void Print(bool detailFlag) override;
};


#endif //MYFSMDEMO_REGFSM_H
