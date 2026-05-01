//
// Created by MR on 2026/4/30.
//
//
//
// Created by MR on 2026/4/30.
//

#ifndef MYFSMDEMO_COMMON_DEF_H
#define MYFSMDEMO_COMMON_DEF_H

#include <vector>
#include <list>

#include "CMsg.h"


#define U32 unsigned int
#define WS_TIMER_ID unsigned int

#define CHAT_SERVICE_KEY_BASE 0
#define MAX_CHAT_SERVICE_KEY 5

enum Tmsg_type
{
    ZERO,
    START,
    END
};

enum Tstate
{
    IDLE = 0,
    WORKING,
    KILL_FSM
};

enum EerrNo
{
    INIT = 0,
    SUCCESS,
    ERROR,
};


#endif //MYFSMDEMO_COMMON_DEF_H