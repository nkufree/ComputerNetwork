#ifndef __SEND_FILE_H__
#define __SEND_FILE_H__


#include "file_trans.h"
#include "sliding_window.h"
#include <fstream>
#include <atomic>

class SendFile : public FileTrans
{
private:
    // int seq_;
    // int ack_;
    int fileSize_;
    char *fileName_;
    SlidingWindow sendWindow_;
    std::ifstream sendFileStream_;
    timeval tv_;
    // std::mutex seq_mutex_;
    // std::mutex ack_mutex_;
    volatile std::atomic_bool send_over_ = false;
    // std::mutex over_mutex_;
    std::mutex mutex_;
    RC init_connect();
    void set_wait_time(int t);
    RC send_file_name(const char* fileName);
    RC recv_message(int &len); // 完成接收消息、检验校验码、检验seq、停等
    void setSendBuf();
    // virtual int getSeq(bool inc = true) override;
    // virtual int getAck() override;
    virtual int getWin() override;
    // void setSeq(uint32_t seq);
    // void setAck(uint32_t ack);
    void setSendOver(bool send_over);
    bool getSendOver();
    virtual Type getType() override
    {
        return FileTrans::Type::F_SEND;
    }
    RC send_and_wait(int len);
    RC disconnect();

public:
    SendFile(const char* sendAddr, const char* recvAddr, int sendPort, int recvPort);
    static void waitACK(SendFile* sf);
    bool init();
    RC setFile(const char* fileName);
    RC start();
    ~SendFile()
    {
        closesocket(sock_);
        WSACleanup();
        sendFileStream_.close();
    }
};

#endif