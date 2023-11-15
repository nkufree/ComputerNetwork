#ifndef __RECV_FILE_H__
#define __RECV_FILE_H__
#pragma comment(lib, "ws2_32.lib")

#include "file_trans.h"
#include <fstream>
#include <vector>

class RecvFile : public FileTrans
{
private:
    int seq_;
    timeval start_, end_;
    std::ofstream recvFileStream_;
    // std::vector<std::pair<timeval, int>> info_;
    RC init_connect();
    RC recv_file_name();
    RC recv_message(int &len); // 完成接收消息、检验校验码、检验seq、消息重传
    virtual int getSeq() override;
    virtual Type getType() override
    {
        return FileTrans::Type::F_RECV;
    }
    RC wait_and_send();
    RC disconnect();
    // void calcInfo();

public:
    RecvFile(const char* sendAddr, const char* recvAddr, int sendPort, int recvPort);
    bool init();
    RC start();
    ~RecvFile()
    {
        if(sendMsg_ != nullptr) delete sendMsg_;
        if(recvMsg_ != nullptr) delete recvMsg_;
        closesocket(sock_);
        WSACleanup();
        recvFileStream_.close();
    }
};

#endif


