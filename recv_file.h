#ifndef __RECV_FILE_H__
#define __RECV_FILE_H__
#pragma comment(lib, "ws2_32.lib")

#include "crc32.h"
#include <winsock2.h>
#include <fstream>
#include "defs.h"

class RecvFile
{
private:
    SOCKET sockRecv_;
    sockaddr_in recvAddr_;
    sockaddr_in sendAddr_;
    fileMessage* sendMsg_;
    fileMessage* recvMsg_;
    int seq_;
    int addrSize_;
    timeval start_, end_;
    std::ofstream recvFileStream_;
    states state_;
    bool init_connect();
    bool recv_file_name();
    bool send_message(int len); // 完成设置seq、计算校验码、发送消息、消息重传
    int recv_message(); // 完成接收消息、检验校验码、检验seq、消息重传
    bool wait_and_send();
    bool disconnect();

public:
    RecvFile(const char* sendAddr, const char* recvAddr, int sendPort, int recvPort);
    bool init();
    bool recv();
    ~RecvFile()
    {
        if(sendMsg_ != nullptr) delete sendMsg_;
        if(recvMsg_ != nullptr) delete recvMsg_;
        closesocket(sockRecv_);
        WSACleanup();
        recvFileStream_.close();
    }
};

#endif


