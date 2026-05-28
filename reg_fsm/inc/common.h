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
// 单个工厂最多管理的 FSM 数量。
#define FSM_NUM_IN_FAC 32

// 单个 manager 最多注册的工厂数量。
#define FAC_NUM_IN_MGR 8

enum Tmsg_type
{
    ZERO,
    START,
    END
};

enum Tstate
{
    IDLE = 0,       // FSM 已创建，尚未进入业务处理。
    WORKING,        // FSM 正在处理业务流程。
    KILL_FSM        // FSM 已结束，等待工厂回收。
};

enum EerrNo
{
    INIT = 0,       // 初始状态或尚未处理。
    SUCCESS,        // 处理成功。
    ERROR,          // 处理失败。
};


#endif //MYFSMDEMO_COMMON_DEF_H
