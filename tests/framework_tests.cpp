//
// Created by MR on 2026/5/28.
//

#include <cassert>
#include <chrono>
#include <thread>

#include "../reg_fsm/inc/CMsg.h"
#include "../reg_fsm/inc/Cfactory_mgr.h"
#include "../reg_fsm/inc/RegFactory.h"
#include "../reg_fsm/inc/RegFsm.h"

void TestDefaultMsg()
{
    // 验证消息对象的默认路由字段和事件类型。
    CMsg msg;
    assert(MSG_INIT == msg.type);
    assert(0 == msg.serviceId);
    assert(0 == msg.fsmId);
    assert(0 == msg.sessionId);
}

void TestInvalidTransition()
{
    // RegFsm 初始为 IDLE，直接处理 MSG_RESP 应被判定为非法转移。
    RegFsm fsm;
    CMsg msg;
    msg.type = MSG_RESP;

    assert(ERROR == fsm.ProcessMsg(msg));
}

void TestManagerFactoryFlow()
{
    // 验证 manager -> factory -> FSM 的主链路可以被驱动起来。
    Cfactory_mgr mgr;
    assert(SUCCESS == mgr.RegisterFactory(new RegFactory(1)));
    assert(SUCCESS == mgr.Start());

    CMsg msg;
    msg.serviceId = 1;
    msg.type = MSG_INIT;
    assert(SUCCESS == mgr.SendMsg(msg));

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    mgr.Stop();
    mgr.Join();
}

int main()
{
    TestDefaultMsg();
    TestInvalidTransition();
    TestManagerFactoryFlow();

    return 0;
}
