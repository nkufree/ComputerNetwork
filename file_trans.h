#ifndef __FILE_TRANS_H__
#define __FILE_TRANS_H__

#include "defs.h"
#include <winsock2.h>
#include <mutex>

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
    int seq_;
    int ack_;
    std::mutex seq_mutex_;
    std::mutex ack_mutex_;
    std::mutex print_mutex_;
    enum Type
    {
        F_SEND,
        F_RECV,
    } ;
public:
    FileTrans(const char* sendAddr, const char* recvAddr, int sendPort, int recvPort);
    FileTrans(){}
    RC open();
    int getSeq(bool inc = true);
    int getAck();
    virtual int getWin() = 0;
    void setSeq(uint32_t seq);
    void setAck(uint32_t ack);
    virtual Type getType() = 0;
    RC recvMsg(int &len);
    RC sendMsg(int len = 0, int seq = -1);
    ~FileTrans();
};




#endif