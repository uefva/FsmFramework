//
// Created by MR on 2026/5/28.
//

#include "../inc/RegFactory.h"

#include <iostream>

#include "../inc/RegFsm.h"

RegFactory::RegFactory(unsigned int facId) : Cfactory(facId)
{
}

Cfsm* RegFactory::CreateFsm()
{
    return new RegFsm();
}
