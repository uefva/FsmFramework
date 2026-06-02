//
// Shared table-driven FSM execution helper.
//

#ifndef MYFSMDEMO_FSMTABLEEXECUTOR_H
#define MYFSMDEMO_FSMTABLEEXECUTOR_H

#include <sstream>

#include "Cfsm.h"
#include "Logger.h"

template <typename TransitionT>
const TransitionT* FindFsmTransition(const TransitionT* transitions,
                                     unsigned int transitionCount,
                                     Tstate state,
                                     MsgType event)
{
    for (unsigned int index = 0; index < transitionCount; ++index)
    {
        if ((transitions[index].from == state) &&
            (transitions[index].event == event))
        {
            return &transitions[index];
        }
    }

    return nullptr;
}

template <typename FsmT, typename TransitionT>
EerrNo ExecuteFsmTransition(FsmT& fsm,
                            CMsg& msg,
                            const TransitionT* transitions,
                            unsigned int transitionCount,
                            const char* fsmName)
{
    if (KILL_FSM == fsm.GetState())
    {
        LOG_WARN(fsmName, "invalid state=" << StateToString(fsm.GetState())
                         << " event=" << MsgTypeToString(msg.type));
        return INVALID_STATE;
    }

    if (SUCCESS != fsm.Cfsm::ProcessMsg(msg))
    {
        LOG_ERROR(fsmName, "base ProcessMsg failed, event="
                           << MsgTypeToString(msg.type));
        return INVALID_MSG;
    }

    const TransitionT* transition =
        FindFsmTransition(transitions, transitionCount, fsm.GetState(), msg.type);
    if (nullptr == transition)
    {
        LOG_WARN(fsmName, "invalid transition, state="
                          << StateToString(fsm.GetState())
                          << " event=" << MsgTypeToString(msg.type));
        return INVALID_MSG;
    }

    std::ostringstream transitionLog;
    transitionLog << "fsm=" << fsm.GetFsmId()
                  << " event=" << MsgTypeToString(msg.type)
                  << " state=" << StateToString(fsm.GetState())
                  << "->" << StateToString(transition->to);
    if (transition->hasNext)
    {
        transitionLog << " next=" << MsgTypeToString(transition->nextEvent);
        if (transition->delayMs > 0)
        {
            transitionLog << " delayMs=" << transition->delayMs;
        }
    }
    else
    {
        transitionLog << " next=none";
    }

    LOG_INFO(fsmName, transitionLog.str());
    fsm.RunAction(transition->action);
    fsm.SetState(transition->to);

    if (transition->hasNext)
    {
        return fsm.PostNextEvent(*transition, msg);
    }

    return SUCCESS;
}

#endif //MYFSMDEMO_FSMTABLEEXECUTOR_H
