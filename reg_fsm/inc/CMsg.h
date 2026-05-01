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
    MsgType type = MSG_INIT;

    std::vector<char> msg;
};


#endif //MYFSMDEMO_CMSG_H