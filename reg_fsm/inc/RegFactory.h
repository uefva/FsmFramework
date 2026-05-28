//
// Created by MR on 2026/5/28.
//

#ifndef MYFSMDEMO_REGFACTORY_H
#define MYFSMDEMO_REGFACTORY_H

#include "Cfactory.h"

// Registration business factory.
// Creates RegFsm instances and routes registration messages by fsmId.
class RegFactory : public Cfactory
{
public:
    explicit RegFactory(unsigned int facId);

    Cfsm* CreateFsm() override;
    EerrNo FacMsgPrc(CMsg& msg) override;
};

#endif //MYFSMDEMO_REGFACTORY_H
