//
// Created by MR on 2026/5/28.
//

#ifndef MYFSMDEMO_REGFACTORY_H
#define MYFSMDEMO_REGFACTORY_H

#include "Cfactory.h"

// 注册业务工厂：
// 负责创建 RegFsm，并按 fsmId 将注册类消息路由到对应实例。
class RegFactory : public Cfactory
{
public:
    explicit RegFactory(unsigned int facId);

    Cfsm* CreateFsm() override;
    EerrNo FacMsgPrc(CMsg& msg) override;
};

#endif //MYFSMDEMO_REGFACTORY_H
