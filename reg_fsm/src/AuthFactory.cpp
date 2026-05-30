//
// Created by MR on 2026/5/29.
//

#include "../inc/AuthFactory.h"

#include <iostream>

#include "../inc/AuthFsm.h"

AuthFactory::AuthFactory(unsigned int facId) : Cfactory(facId)
{
}

Cfsm* AuthFactory::CreateFsm()
{
    return new AuthFsm();
}
