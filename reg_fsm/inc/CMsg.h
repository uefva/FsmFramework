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
    // Current event type used by the FSM to find a transition rule.
    MsgType type = MSG_INIT;

    // Target factory ID used by Cfactory_mgr to route the message.
    unsigned int serviceId = 0;

    // Target FSM instance ID. A value of 0 may create a new FSM by factory rules.
    unsigned int fsmId = 0;

    // Business session ID reserved for upper-layer protocol or workflow tracking.
    unsigned int sessionId = 0;

    // Message payload. The current demo does not deeply use it yet.
    std::vector<char> msg;
};


#endif //MYFSMDEMO_CMSG_H
