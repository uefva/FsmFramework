//
// Created by MR on 2026/4/29.
//

#include "inc/RegFsm.h"

void FsmTest1()
{
    RegFsm Creg_fsm;
    CMsg pMsg;

    Creg_fsm.PrePrcMsg(pMsg);

    while (KILL_FSM != Creg_fsm.GetState())
    {
        Creg_fsm.ProcessMsg(pMsg);
    }

    Creg_fsm.PostPrcMsg(pMsg);
}

int main() {
    FsmTest1();

    return 0;
}
