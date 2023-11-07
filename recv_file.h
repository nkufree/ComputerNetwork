#ifndef __RECV_FILE_H__
#define __RECV_FILE_H__
#pragma comment(lib, "ws2_32.lib")

#include "file_trans.h"
#include "sliding_window.h"
#include <fstream>

class RecvFile : public FileTrans
{
private:
    int seq_;
    timeval start_, end_;
    std::ofstream recvFileStream_;
    SlidingWindow recvWindow_;
    bool recv_over_;
    std::mutex over_mutex_;
    RC init_connect();
    RC recv_file_name();
    RC recv_message(int &len); // 完成接收消息、检验校验码、检验seq、消息重传
    // virtual int getSeq(bool inc = true) override;
    virtual int getWin() override;
    virtual Type getType() override
    {
        return FileTrans::Type::F_RECV;
    }
    void setRecvOver(bool recv_over);
    bool getRecvOver();
    static void writeInDisk(RecvFile* rf);
    RC wait_and_send();
    RC disconnect();

public:
    RecvFile(const char* sendAddr, const char* recvAddr, int sendPort, int recvPort);
    bool init();
    RC start();
    ~RecvFile()
    {
        closesocket(sock_);
        WSACleanup();
        recvFileStream_.close();
    }
};

#endif


