#ifndef __SEND_FILE_H__
#define __SEND_FILE_H__


#include "file_trans.h"
#include <fstream>

class SendFile : public FileTrans
{
private:
    int seq_;
    int fileSize_;
    char *fileName_;
    std::ifstream sendFileStream_;
    timeval tv_;
    RC init_connect();
    void set_wait_time(int t);
    RC send_file_name(const char* fileName);
    RC recv_message(int &len); // 完成接收消息、检验校验码、检验seq、停等
    virtual int getSeq() override;
    virtual Type getType() override
    {
        return FileTrans::Type::F_SEND;
    }
    RC send_and_wait(int len);
    RC disconnect();

public:
    SendFile(const char* sendAddr, const char* recvAddr, int sendPort, int recvPort);
    bool init();
    RC setFile(const char* fileName);
    RC start();
    void setLoss(double loss){loss_num_ = 100 / loss;}
    void setDelay(int delay){delay_ = delay;}
    ~SendFile()
    {
        if(sendMsg_ != nullptr) delete sendMsg_;
        if(recvMsg_ != nullptr) delete recvMsg_;
        closesocket(sock_);
        WSACleanup();
        sendFileStream_.close();
    }
};

#endif