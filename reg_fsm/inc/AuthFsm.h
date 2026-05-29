//
// Created by MR on 2026/5/29.
//

#ifndef MYFSMDEMO_AUTHFSM_H
#define MYFSMDEMO_AUTHFSM_H

#include "Cfsm.h"

class AuthFsm : public Cfsm
{
private:
    EerrNo SendNextMsg(const CMsg& currentMsg, MsgType nextType);

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
