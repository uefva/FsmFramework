//
// Created by MR on 2026/4/30.
//

#ifndef MYFSMDEMO_CMSG_H
#define MYFSMDEMO_CMSG_H
#include <vector>

enum MsgType
{
    MSG_INIT,
    MSG_CONNECT,
    MSG_REQ,
    MSG_RESP,
    MSG_TIMEOUT,
    MSG_CLOSE
};

class CMsg
{
    public:
    // 当前事件类型，状态机根据该字段查找状态转移规则。
    MsgType type = MSG_INIT;

    // 目标工厂 ID，用于 Cfactory_mgr 将消息路由到对应 Cfactory。
    unsigned int serviceId = 0;

    // 目标 FSM 实例 ID。为 0 时，具体工厂可以按业务规则创建新 FSM。
    unsigned int fsmId = 0;

    // 业务会话 ID，预留给上层协议或业务流程做关联。
    unsigned int sessionId = 0;

    // 消息载荷，当前 demo 未深度使用，后续可承载协议数据。
    std::vector<char> msg;
};


#endif //MYFSMDEMO_CMSG_H
