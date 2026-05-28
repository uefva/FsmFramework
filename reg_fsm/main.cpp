//
// Created by MR on 2026/4/29.
//

#include <chrono>
#include <iostream>
#include <ostream>
#include <thread>

#include "inc/Cfactory_mgr.h"
#include "inc/RegFactory.h"

Cfactory_mgr mgr;

void FsmMgrTest(const U32 serviceId)
{
    CMsg pMsg;
    // Set the second-level factory id.
    pMsg.serviceId = serviceId;
    pMsg.type = MSG_INIT;

    // Only send the first message; RegFsm drives the rest via manager events or timers.
    mgr.SendMsg(pMsg);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

int main() {
    mgr.RegisterFactory(new RegFactory(FAC_REG_FAC_ID));
    mgr.RegisterFactory(new RegFactory(FAC_AUTH_FAC_ID));

    // Let the manager own its background message-pump thread.
    mgr.Start();

    std::cout << std::endl;

    // Send messages to two different target factories.
    FsmMgrTest(FAC_REG_FAC_ID);
    FsmMgrTest(FAC_AUTH_FAC_ID);

    {
        // FAC_AUTH_FAC_ID + 1 is an unknown factory id and should trigger an error path.
        FsmMgrTest(FAC_AUTH_FAC_ID + 1);
    }


    mgr.Stop();
    mgr.Join();

    return 0;
}
