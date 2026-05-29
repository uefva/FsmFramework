//
// Shared table-driven FSM execution helper.
//

#ifndef MYFSMDEMO_FSMTABLEEXECUTOR_H
#define MYFSMDEMO_FSMTABLEEXECUTOR_H

#include <iostream>

#include "Cfsm.h"

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
        return ERROR;
    }

    if (SUCCESS != fsm.Cfsm::ProcessMsg(msg))
    {
        std::cout << fsmName << "::ProcessMsg error" << std::endl;
        return EerrNo::ERROR;
    }

    const TransitionT* transition =
        FindFsmTransition(transitions, transitionCount, fsm.GetState(), msg.type);
    if (nullptr == transition)
    {
        std::cout << fsmName << "::ProcessMsg invalid transition, state="
                  << fsm.GetState() << " msg=" << msg.type << std::endl;
        return EerrNo::ERROR;
    }

    std::cout << transition->log << std::endl;
    fsm.RunAction(transition->action);
    fsm.SetState(transition->to);

    if (transition->hasNext)
    {
        return fsm.PostNextEvent(*transition, msg);
    }

    return SUCCESS;
}

#endif //MYFSMDEMO_FSMTABLEEXECUTOR_H
