//
// Created by MR on 2026/5/28.
//

#include <cassert>
#include <chrono>
#include <thread>

#include "../reg_fsm/inc/AuthFactory.h"
#include "../reg_fsm/inc/AuthFsm.h"
#include "../reg_fsm/inc/CMsg.h"
#include "../reg_fsm/inc/Cfactory_mgr.h"
#include "../reg_fsm/inc/Logger.h"
#include "../reg_fsm/inc/RegFactory.h"
#include "../reg_fsm/inc/RegFsm.h"

void TestDefaultMsg()
{
    CMsg msg;
    assert(MSG_INIT == msg.type);
    assert(0 == msg.serviceId);
    assert(0 == msg.fsmId);
    assert(0 == msg.sessionId);
}

void TestInvalidTransition()
{
    RegFsm fsm;
    CMsg msg;
    msg.type = MSG_RESP;

    assert(INVALID_MSG == fsm.ProcessMsg(msg));
}

void TestAuthInvalidTransition()
{
    AuthFsm fsm;
    CMsg msg;
    msg.type = MSG_RESP;

    assert(INVALID_MSG == fsm.ProcessMsg(msg));
}

void TestManagerFactoryFlow()
{
    Cfactory_mgr mgr;
    assert(SUCCESS == mgr.RegisterFactory(new RegFactory(FAC_REG_FAC_ID)));
    assert(SUCCESS == mgr.RegisterFactory(new AuthFactory(FAC_AUTH_FAC_ID)));
    assert(SUCCESS == mgr.Start());

    CMsg msg;
    msg.serviceId = FAC_REG_FAC_ID;
    msg.type = MSG_INIT;
    assert(SUCCESS == mgr.SendMsg(msg));

    CMsg authMsg;
    authMsg.serviceId = FAC_AUTH_FAC_ID;
    authMsg.type = MSG_INIT;
    assert(SUCCESS == mgr.SendMsg(authMsg));

    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    mgr.Stop();
}

int main()
{
    Logger::Instance().SetLevel(LogLevel::OFF);

    TestDefaultMsg();
    TestInvalidTransition();
    TestAuthInvalidTransition();
    TestManagerFactoryFlow();

    return 0;
}
