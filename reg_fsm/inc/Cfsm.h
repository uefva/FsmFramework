//
// Created by MR on 2026/4/29.
//

#ifndef MYFSMDEMO_REG_FSM_H
#define MYFSMDEMO_REG_FSM_H

#include "common.h"

class Cfsm
{
private:
    Tstate _state;      //State of FSM
    EerrNo _prc;        //Message processed result

public:
    void _changeState(Tstate state);            //Change state of FSM

    // std::vector<char> _getMsgBuf(U32 MsgLen);
    //                                             //Get message buffer from factory manager's repository
    // EerrNo _freeMsgBuf(std::vector<char>& pBuf);          //Free message buffer from factory manager's repository
    // EerrNo _sendMsg(std::vector<char>& pBuf);             //Send message

public:
    explicit Cfsm(Tstate state = IDLE);          //Constructor
    virtual ~Cfsm();                            //Destructor

    Tstate GetState() const;                          //Get state of FSM
    void SetState(Tstate state);                //Set state of FSM

    virtual void PrePrcMsg(CMsg& pBuf) = 0;             //Pre Process message
    virtual EerrNo ProcessMsg(CMsg& pMsg) = 0;    //Process message
    virtual void PostPrcMsg(CMsg& pBuf) = 0;            //Post Process message

    virtual EerrNo Create();                //Called by factory when FSM created
    virtual EerrNo Destory();               //Called by factory when FSM destory

    virtual void Print(bool detailFlag);         //Print information
};


#endif //MYFSMDEMO_REG_FSM_H