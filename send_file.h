#ifndef __SEND_FILE_H__
#define __SEND_FILE_H__
#pragma comment(lib, "ws2_32.lib")

#include "crc32.h"
#include <winsock2.h>
#include "defs.h"
#include <fstream>

class SendFile
{
private:
    SOCKET sockSend_;
    sockaddr_in recvAddr_;
    sockaddr_in sendAddr_;
    fileMessage* sendMsg_;
    fileMessage* recvMsg_;
    int seq_;
    int fileSize_;
    std::ifstream sendFileStream_;
    int addrSize_;
    timeval tv_;
    states state_;
    bool init_connect();
    void set_wait_time(int t);
    bool send_file_name(const char* fileName);
    bool send_message(int len); // 完成设置seq、计算校验码、发送消息
    bool resend_message(int len); // 消息重传
    int recv_message(); // 完成接收消息、检验校验码、检验seq、停等
    int recv_overtime();
    int get_seq();
    bool send_and_wait(int len);
    bool disconnect();

public:
    SendFile(const char* sendAddr, const char* recvAddr, int sendPort, int recvPort);
    bool init();
    bool send(const char* fileName);
    ~SendFile()
    {
        if(sendMsg_ != nullptr) delete sendMsg_;
        if(recvMsg_ != nullptr) delete recvMsg_;
        closesocket(sockSend_);
        WSACleanup();
        sendFileStream_.close();
    }
};

#endif