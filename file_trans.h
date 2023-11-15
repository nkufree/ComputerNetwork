#ifndef __FILE_TRANS_H__
#define __FILE_TRANS_H__

#include "defs.h"
#include <winsock2.h>

class FileTrans
{
protected:
    SOCKET sock_;
    sockaddr_in recvAddr_;
    sockaddr_in sendAddr_;
    fileMessage* sendMsg_;
    fileMessage* recvMsg_;
    states state_;
    int addrSize_;
    enum Type
    {
        F_SEND,
        F_RECV,
    } ;
    int loss_num_;
    int loss_count_;
    int delay_;
    int delay_count_;
public:
    FileTrans(/* args */);
    RC open();
    virtual int getSeq() = 0;
    virtual Type getType() = 0;
    RC recvMsg(int &len);
    RC sendMsg(int len = 0, int seq = -1);
    ~FileTrans();
};




#endif