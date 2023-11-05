#include "recv_file.h"
#include <iostream>
#include <sys/time.h>
#include <io.h>
using namespace std;

RecvFile::RecvFile(const char* sendAddr, const char* recvAddr, int sendPort, int recvPort) : FileTrans(sendAddr, recvAddr, sendPort, recvPort)
{
}

bool RecvFile::init()
{
    WSADATA wsa_data;
    if(WSAStartup(MAKEWORD(2,2), &wsa_data) != 0)
    {
        cout << "初始化网络错误" << endl;
        return 0;
    }
    if((sock_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
    {
        cout << "初始化套接字错误" << endl;
        return 0;
    }
    if(bind(sock_, (sockaddr*)&recvAddr_, sizeof(recvAddr_)) == SOCKET_ERROR)
    {
        cout << "绑定端口错误" << endl;
        return 0;
    }
    sendMsg_ = new fileMessage;
    recvMsg_ = new fileMessage;
    addrSize_ = sizeof(sendAddr_);
    seq_ = 1;
    return 1;
}


RC RecvFile::init_connect()
{
    int len;
    RC rc;
    while(state_ != ESTABLISHED)
    {
        switch(state_)
        {
        case LISTEN:
            rc = recv_message(len);
            LOG_MSG(rc, "第一次握手成功", "第一次握手失败");
            gettimeofday(&start_, NULL);
            rc = sendMsg();
            break;
        case SYN_RCVD:
            rc = recv_message(len);
            LOG_MSG(rc, "第三次握手成功", "第三次握手失败");
            break;
        default:
            break;
        }
        if(recvMsg_->head.flag == PSH)
        {
            sendMsg_->head.flag = RST;
            sendMsg();
            state_ = LISTEN;
        }
    }
    return rc;
}

int RecvFile::getSeq()
{
    return seq_;
}

RC RecvFile::recv_message(int &len)
{
    RC rc;
    while(true)
    {
        rc = recvMsg(len);
        if(rc != RC::SUCCESS)
            return rc;
        if(recvMsg_->head.seq == seq_)
        {
            sendMsg_->head.flag = ACK;
            rc = sendMsg();
            if(rc != RC::SUCCESS)
                return rc;
            continue;
        }
        break;
    }
    seq_ = recvMsg_->head.seq;
    return rc;
}

RC RecvFile::recv_file_name()
{
    int len;
    RC rc;
    rc = recv_message(len);
    LOG_MSG(rc, "", "与发送方断开连接");

    ((char*)recvMsg_)[len] = '\0';
    seq_ = recvMsg_->head.seq;
    cout << "收到文件名：" << recvMsg_->msg << endl;
    sendMsg_->head.flag = ACK;
    rc = sendMsg();
    if(access("recv", 0) == -1)
    {
        system("md recv");
    }
    char recvDir[MSS + 8] = {"recv\\"};
    strcat(recvDir, recvMsg_->msg);
    recvFileStream_.open(recvDir, ios::binary);
    return rc;
}

RC RecvFile::wait_and_send()
{
    int len;
    RC rc;
    while(true)
    {
        rc = recv_message(len);
        LOG_MSG(rc, "", "与发送方断开连接");
        if(recvMsg_->head.flag == PSH)
        {
            seq_ = recvMsg_->head.seq;
            recvFileStream_.write(recvMsg_->msg, len - sizeof(info));
            recvFileStream_.flush();
            sendMsg_->head.flag = ACK;
            RC rc;
            rc = sendMsg();
            LOG_MSG(rc, "", "发送消息失败");
        }
        else if(recvMsg_->head.flag == FIN)
        {
            return disconnect();
        }
    }
    return rc;
}

RC RecvFile::disconnect()
{
    int len;
    RC rc;
    timeval tv = { WAIT_TIME / 1000, WAIT_TIME % 1000 * 1000};
    while (state_ != CLOSED)
    {
        switch (state_)
        {
        case ESTABLISHED:
            sendMsg_->head.flag = ACK;
            rc = sendMsg();
            LOG_MSG(rc, "第二次挥手成功", "第二次挥手失败");
            break;
        case CLOSE_WAIT:
            sendMsg_->head.flag = FIN | ACK;
            rc = sendMsg();
            LOG_MSG(rc, "第三次挥手成功", "第三次挥手失败");
            break;
        case LAST_ACK:
            fd_set rset;
            FD_ZERO(&rset);
            FD_SET(sock_, &rset);
            if(select(sock_ + 1, &rset, NULL, NULL, &tv) > 0)
            {
                RC rc = recvMsg(len);
                if(rc != RC::SUCCESS)
                    return rc;
            }
            LOG_MSG(rc, "第四次挥手成功\n关闭连接成功", "第四次挥手失败");
            break;
        default:
            break;
        }

    }
    return RC::SUCCESS;
}

RC RecvFile::start()
{
    this->open();
    RC rc;
    rc = init_connect();
    LOG_MSG(rc, "建立连接成功", "建立连接失败")
    
    rc = recv_file_name();
    if(rc != RC::SUCCESS)
        return rc;
    
    rc = wait_and_send();
    if(rc != RC::SUCCESS)
        return rc;
    gettimeofday(&end_, NULL);
    cout << "接收等待时长为 " << end_.tv_sec - start_.tv_sec + (end_.tv_usec - start_.tv_usec) / 1000000.0 << " s" << endl;
    return RC::SUCCESS;
}