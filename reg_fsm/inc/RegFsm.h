//
// Created by MR on 2026/4/30.
//

#ifndef MYFSMDEMO_REGFSM_H
#define MYFSMDEMO_REGFSM_H
#include "Cfsm.h"


class RegFsm : public Cfsm
{
public:
    explicit RegFsm();

    void PrePrcMsg(CMsg& pBuf);
    EerrNo ProcessMsg(CMsg& pMsg);
    void PostPrcMsg(CMsg& pBuf);

    EerrNo Destory() override;
    void Print(bool detailFlag) override;
};


#endif //MYFSMDEMO_REGFSM_H