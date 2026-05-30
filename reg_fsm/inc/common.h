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

// Maximum number of FSM instances managed by one factory.
#define FSM_NUM_IN_FAC 32

constexpr U32 FAC_REG_FAC_ID        = 1;     // Registration factory.
constexpr U32 FAC_AUTH_FAC_ID       = 3;     // Authentication factory.
constexpr U32 FAC_NUM_IN_MGR_MAX    = 8;     // Maximum number of factories in one manager.


enum Tmsg_type
{
    ZERO,
    START,
    END
};

enum Tstate
{
    IDLE = 0,       // FSM is created but has not entered business processing.
    WORKING,        // FSM is processing the business flow.
    KILL_FSM        // FSM is finished and waiting for factory cleanup.
};

enum EerrNo
{
    INIT = 0,        // Initial or not-yet-processed result.
    SUCCESS,         // Processing succeeded.
    ERROR,           // General processing failure.
    INVALID_STATE,   // FSM is in a state that cannot handle this message.
    INVALID_MSG,     // Message type is not valid for the current FSM state.
    TIMER_ERROR,     // Timer operation failed.
};


#endif //MYFSMDEMO_COMMON_DEF_H
