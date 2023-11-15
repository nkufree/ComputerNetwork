#include "recv_file.h"
#include <iostream>
#include <sys/time.h>
#include <io.h>
#include <thread>
using namespace std;

RecvFile::RecvFile(const char* sendAddr, const char* recvAddr, int sendPort, int recvPort) : FileTrans(sendAddr, recvAddr, sendPort, recvPort), recvWindow_(BUFF_SIZE, RECV_WINDOW_SIZE)
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
    seq_ = 0;
    ack_ = 0;
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
            rc = recvMsg(len);
            LOG_MSG(rc, "第一次握手成功", "第一次握手失败");
            gettimeofday(&start_, NULL);
            rc = sendMsg();
            break;
        case SYN_RCVD:
            rc = recvMsg(len);
            LOG_MSG(rc, "第三次握手成功", "第三次握手失败");
            break;
        default:
            break;
        }
        if(recvMsg_->head.flag == PSH && state_ != ESTABLISHED)
        {
            sendMsg_->head.flag = RST;
            sendMsg();
            state_ = LISTEN;
        }
    }
    return rc;
}

int RecvFile::getWin()
{
    return RECV_WINDOW_SIZE;
}

RC RecvFile::recv_message(int &len)
{
    RC rc;
    rc = recvMsg(len);
    if(rc != RC::SUCCESS)
        return rc;
    sendMsg_->head.flag = ACK;
    rc = sendMsg();
    return rc;
}

RC RecvFile::recv_file_name()
{
    int len;
    RC rc;
    rc = recv_message(len);
    LOG_MSG(rc, "", "与发送方断开连接");

    ((char*)recvMsg_)[len] = '\0';
    cout << "收到文件名：" << recvMsg_->msg << endl;
    sendMsg_->head.flag = ACK;
    sendMsg_->head.ack = recvMsg_->head.seq + 1;
    rc = sendMsg();
    if(access("recv", 0) == -1)
    {
        system("md recv");
    }
    char recvDir[MSS + 8] = {"recv\\"};
    strcat(recvDir, recvMsg_->msg);
    recvFileStream_.open(recvDir, ios::binary);
    recvWindow_.setStartSeq(recvMsg_->head.seq + 1);
    return rc;
}

void RecvFile::setRecvOver(bool recv_over)
{
    // lock_guard<mutex> lock(over_mutex_);
    recv_over_ = recv_over;
}

bool RecvFile::getRecvOver()
{
    // lock_guard<mutex> lock(over_mutex_);
    return recv_over_;
}

void RecvFile::writeInDisk(RecvFile* rf)
{
    SlidingWindow& sw = rf->recvWindow_;
    // ofstream write_txt("a.log");
    int t = 0;
    while(!(rf->getRecvOver() && sw.getStart() == sw.getEnd()))
    {
        Sleep(WRITE_FILE_TIME);
        while (sw.getStart() != sw.getNextSend())
        {
            fileMessage& msg = sw.sw_[sw.getStart()];
            // write_txt << t++ << "  " << msg.head.len << "  " << *((int*)msg.msg) << endl;
            rf->recvFileStream_.write(msg.msg, msg.head.len);
            sw.movePos(S_START, 1);
            // sw.movePos(S_END, 1);
        }
        rf->recvFileStream_.flush();
    }
    // write_txt.close();
}

RC RecvFile::wait_and_send()
{
    int len;
    RC rc;
    timeval start, end;
    
    gettimeofday(&start, NULL);
    while(true)
    {
        fd_set rset;
        FD_ZERO(&rset);
        FD_SET(sock_, &rset);
        timeval tv = MS_TO_TIMEVAL(WAIT_TIME);
        if(select(sock_ + 1, &rset, NULL, NULL, &tv) > 0)
        {
            rc = recvMsg(len);
        }
        LOG_MSG(rc, "", "与发送方断开连接");
        if(recvMsg_->head.flag == PSH)
        {
            timeval tmp;
            gettimeofday(&tmp, NULL);
            info_.push_back(pair(tmp, recvWindow_.getNext() - recvWindow_.getLossNum()));
            uint32_t &seq = recvMsg_->head.seq;
            recvWindow_.updateMsg(recvMsg_);
            // recvWindow_.printAckQuene();
            // recvWindow_.printSliding();
            gettimeofday(&end, NULL);
            // if(TIMEVAL_GAP(end, start) > DELAY_ACK_TIME)
            {
                RC rc;
                // sendMsg_->head.ack = seq + 1;
                setAck(recvWindow_.getNextAck());
                rc = sendMsg();
                LOG_MSG(rc, "", "发送消息失败");
                start = end;
            }
        }
        else if(recvMsg_->head.flag == FIN)
        {
            setRecvOver(true);
            recvWindow_.setPos(S_END, recvWindow_.getNext());
            return disconnect();
        }
    }
    
    return rc;
}

RC RecvFile::disconnect()
{
    int len;
    RC rc;
    timeval tv = MS_TO_TIMEVAL(WAIT_TIME);
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
            state_ = CLOSED;
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
    
    recv_over_ = false;
    sendMsg_->head.flag = ACK;
    thread write_in_disk(writeInDisk, this);
    timeval file_start;
    gettimeofday(&file_start, NULL);
    info_.push_back(pair(file_start, 0));
    rc = wait_and_send();
    write_in_disk.join();
    if(rc != RC::SUCCESS)
        return rc;
    gettimeofday(&end_, NULL);
    cout << "接收等待时长为 " << end_.tv_sec - start_.tv_sec + (end_.tv_usec - start_.tv_usec) / 1000000.0 << " s" << endl;
    calcInfo();
    return RC::SUCCESS;
}

void RecvFile::calcInfo()
{
    timeval last_time = info_.begin()->first;
    int last_num = 0;
    ofstream tuntu("throughput.csv");
    tuntu << "延时(ms),吞吐率(Mbps)" << endl;
    for(auto p : info_)
    {
        int time_us = (p.first.tv_sec - last_time.tv_sec) * 1000000 + p.first.tv_usec - last_time.tv_usec;
        tuntu << time_us / 1000.0 << "," << (p.second - last_num) * 8.0 * MSS/ time_us << endl;
        last_time = p.first;
        last_num = p.second;
    }
}