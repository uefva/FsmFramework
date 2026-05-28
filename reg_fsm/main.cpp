//
// Created by MR on 2026/4/29.
//

#include <chrono>
#include <thread>

#include "inc/Cfactory_mgr.h"
#include "inc/RegFactory.h"

void FsmTest1()
{
    Cfactory_mgr mgr;
    mgr.RegisterFactory(new RegFactory(1));

    // manager 自己启动后台消息泵，调用方不直接管理工作线程。
    mgr.Start();

    CMsg pMsg;
    pMsg.serviceId = 1;
    pMsg.type = MSG_INIT;

    // 只投递首条消息，后续流程由 RegFsm 通过 manager 自投递或定时器推进。
    mgr.SendMsg(pMsg);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    mgr.Stop();
    mgr.Join();
}

int main() {
    FsmTest1();

    return 0;
}
