//
// Created by MR on 2026/5/29.
//

#ifndef MYFSMDEMO_AUTHFACTORY_H
#define MYFSMDEMO_AUTHFACTORY_H

#include "Cfactory.h"

// Authentication business factory.
// Creates AuthFsm instances and routes authentication messages by fsmId.
class AuthFactory : public Cfactory
{
public:
    explicit AuthFactory(unsigned int facId);

    Cfsm* CreateFsm() override;
};

#endif //MYFSMDEMO_AUTHFACTORY_H
