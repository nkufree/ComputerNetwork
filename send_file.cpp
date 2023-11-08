#include "send_file.h"
#include <iostream>
#include <time.h>
#include <sys/time.h>
#include <thread>

using namespace std;

SendFile::SendFile(const char* sendAddr, const char* recvAddr, int sendPort, int recvPort) : FileTrans(sendAddr, recvAddr, sendPort, recvPort), sendWindow_(BUFF_SIZE, WINDOW_SIZE)
{
}

bool SendFile::init()
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
    addrSize_ = sizeof(recvAddr_);
    seq_ = 0;
    ack_ = 0;
    set_wait_time(WAIT_TIME);
    return 1;
}

void SendFile::set_wait_time(int t)
{
    tv_ = {t / 1000, t % 1000 * 1000};
}

RC SendFile::init_connect()
{
    RC rc;
    int len;
    sendMsg_->head.flag = SYN;
    rc = sendMsg();
    LOG_MSG(rc, "第一次握手成功", "第一次握手失败");

    while(state_ != ESTABLISHED)
    {
        switch(state_)
        {
            case LISTEN:
                sendMsg_->head.flag = SYN;
                rc = sendMsg();
            case SYN_SENT:
                rc = recv_message(len);
            if(rc == RC::WAIT_TIME_ERROR)
            {
                state_ = LISTEN;
                break;
            }
            else
                LOG_MSG(rc, "第二次握手成功", "第二次握手失败");
            rc = sendMsg();
            LOG_MSG(rc, "第三次握手成功\n建立连接成功", "第一次握手失败");
            break;
            default:
                break;
        }
    }
    return RC::SUCCESS;
}

RC SendFile::recv_message(int &len)
{
    fd_set rset;
    FD_ZERO(&rset);
    FD_SET(sock_, &rset);
    if(select(sock_ + 1, &rset, NULL, NULL, &tv_) > 0)
    {
        RC rc = recvMsg(len);
        if(rc != RC::SUCCESS)
            return rc;
    }
    else
    {
        return RC::WAIT_TIME_ERROR;
    }
    if(recvMsg_->head.ack != (sendMsg_->head.seq + 1))
        return RC::SEQ_ERROR;
    return RC::SUCCESS;
}

RC SendFile::send_file_name(const char* fileName)
{
    sendMsg_->head.flag = PSH;
    string path = fileName;
    size_t found = path.find_last_of("/\\");
    string file_name = path.substr(found + 1);
    strcpy(sendMsg_->msg, file_name.c_str());
    RC rc = send_and_wait(strlen(sendMsg_->msg));
    LOG_MSG(rc, "", "发送文件名错误");
    return RC::SUCCESS;
}

int SendFile::getWin()
{
    return sendWindow_.getWindow();
}

RC SendFile::send_and_wait(int len)
{
    timeval start, center, end;
    gettimeofday(&start, NULL);
    RC rc;
    rc = sendMsg(len);
    LOG_MSG(rc, "", "与接收端失去连接");
    gettimeofday(&center, NULL);
    int recv_len, retry_times = 0;
    while (true)
    {
        if(retry_times >= MAX_RETRY_TIMES)
        {
            cout << "重传次数过多" << endl;
            return RC::INTERNAL;
        }
        RC rc;
        rc = recv_message(recv_len);
        if(rc == RC::SOCK_ERROR)
        {
            cout << "接受数据错误" << endl;
            return rc;
        }
        else if(rc == RC::SEQ_ERROR)
        {
            // cout << "丢弃ACK" << endl;
            continue;
        }
        else if(rc == RC::CHECK_ERROR)
        {
            // cout << "校验错误重传"  << endl;
            sendMsg(len, sendMsg_->head.seq);
            continue;
        }
        else if(rc == RC::WAIT_TIME_ERROR)
        {
            // cout << "等待时间过长重传"  << endl;
            // cout << tv_.tv_sec << endl;
            retry_times++;
            if(retry_times != 5)
            {
                tv_.tv_sec *= 2;
                tv_.tv_usec *= 2;
            }
            else if(tv_.tv_sec == 0)
            {
                tv_.tv_sec = 1;
                tv_.tv_usec = 0;
            }
            sendMsg(len, sendMsg_->head.seq);
            continue;
        }
        else if(rc == RC::RESET)
            return rc;
        break;
    }
    gettimeofday(&end, NULL);
    // double timec = (end.tv_sec - center.tv_sec) * 1000000 + end.tv_usec - center.tv_usec;
    // if(len > 0 && timec != 0)
    // {
    //     ofstream outtime("tuntu.txt", ios::app);
    //     ofstream outaa("RTT.txt", ios::app);
    //     outaa << (end.tv_sec - center.tv_sec) * 1000000 + end.tv_usec - center.tv_usec << ",";
    //     outtime << len * 8.0 / ((end.tv_sec - start.tv_sec) * 1000000 + end.tv_usec - start.tv_usec) << ",";
    // }
    
    if(len >= 0)
    {
        if(end.tv_sec == start.tv_sec)
        {
            tv_.tv_sec = 0;
            tv_.tv_usec = (end.tv_usec - start.tv_usec) * 1.2;
        }
        else
        {
            tv_.tv_sec = (end.tv_sec - start.tv_sec - 1) * 1.2;
            tv_.tv_usec = (1000000 + end.tv_usec - start.tv_usec) * 1.2;
        }
        if(end.tv_usec-start.tv_usec == 0)
            tv_.tv_usec = 2;
    }
    return RC::SUCCESS;
}

RC SendFile::disconnect()
{
    RC rc = RC::SUCCESS;
    int len;
    sendMsg_->head.flag = FIN;
    rc = sendMsg();
    LOG_MSG(rc, "第一次挥手成功", "第一次挥手失败");
    while(state_ != CLOSED)
    {
        switch(state_)
        {
        case FIN_WAIT_1:
            rc = recv_message(len);
            if(rc == RC::WAIT_TIME_ERROR)
            {
                state_ = ESTABLISHED;
                break;
            }
            else
                LOG_MSG(rc, "第二次挥手成功", "第二次挥手失败");
            break;
        case FIN_WAIT_2:
            rc = recv_message(len);
            if(rc == RC::WAIT_TIME_ERROR)
            {
                state_ = ESTABLISHED;
                break;
            }
            else
                LOG_MSG(rc, "第三次挥手成功", "第三次挥手失败");
            rc = sendMsg();
            LOG_MSG(rc, "第四次挥手成功\n关闭连接成功", "第四次挥手失败");
            break;
        case ESTABLISHED:
            sendMsg_->head.flag = FIN;
            rc = sendMsg();
            break;
        default:
            break;
        }
    }
    return rc;
}

RC  SendFile::setFile(const char* fileName)
{
    sendFileStream_.open(fileName, ios::binary);
    sendFileStream_.seekg(0, ios::end);
    fileSize_ = sendFileStream_.tellg();
    cout << "发送文件大小为 " << fileSize_ << " B" << endl;
    sendFileStream_.seekg(0, ios::beg);
    fileName_ = strdup(fileName);
    return RC::SUCCESS;
}

void SendFile::setSendOver(bool send_over)
{
    // lock_guard<mutex> lock(over_mutex_);
    send_over_ = send_over;
}

bool SendFile::getSendOver()
{
    // lock_guard<mutex> lock(over_mutex_);
    return send_over_;
}

void SendFile::waitACK(SendFile* sf)
{
    int len;
    int ack_last = -1;
    int repeat_time = 1;
    SlidingWindow& sw = sf->sendWindow_;
    while(!sf->getSendOver())
    {
        sf->recvMsg(len);
        if(sf->recvMsg_->head.flag != ACK)
            continue;
        sw.setWindow(sf->recvMsg_->head.win);
        if(sf->recvMsg_->head.ack != ack_last)
        {
            ack_last = sf->recvMsg_->head.ack;
            sw.movePos(S_START, ack_last - sw.getStartSeq());
        }
        else
        {
            repeat_time++;
            if(repeat_time == 3)
            {
                if(ack_last < sw.getNextSeq())
                {
                    sf->mutex_.lock();
                    sw.setPos(S_NEXT, sw.getIndexBySeq(ack_last));
                    sf->setSeq(ack_last);
                    sw.addLossAck(ack_last);
                    sf->mutex_.unlock();
                }
                repeat_time = 1;
            }
        }
        if(ack_last == sw.getSeqByIndex(sw.getDataEnd()))
            sf->setSendOver(true);
        // sw.printSliding();
    }
}

void SendFile::setSendBuf()
{
    int wait_set_len = fileSize_;
    sendWindow_.setStartSeq(seq_);
    int index = 0;
    // ofstream write_txt("b.log");
    while (wait_set_len > 0)
    {
        int set_len = wait_set_len <= MSS ? wait_set_len : MSS;
        sendWindow_.setFlag(index, PSH);
        sendFileStream_.read(sendWindow_.sw_[index].msg, set_len);
        sendWindow_.sw_[index].head.len = set_len;
        // write_txt << index << "  " << set_len << "  " << *((int*)sendWindow_.sw_[index].msg) << endl;
        index++;
        wait_set_len -= set_len;
    }
    // write_txt.close();
    sendWindow_.setDataEnd(index);
}

RC SendFile::start()
{
    this->open();
    timeval start, end;
    RC rc;
    gettimeofday(&start, NULL);
    int try_connect = 0;
    while(init_connect() != RC::SUCCESS)
    {
        try_connect++;
        if(try_connect == MAX_RETRY_TIMES)
        {
            cout << "尝试连接失败" << endl;
            return RC::INTERNAL;
        }
        cout << "尝试重新连接" << endl;
    }
    rc = send_file_name(fileName_);
    if(rc == RC::RESET)
        return this->start();
    else if(rc != RC::SUCCESS)
        return RC::INTERNAL;
    setSendBuf();
    thread wait_ACK(waitACK, this);
    while(!getSendOver())
    {
        mutex_.lock();
        volatile int send_index = sendWindow_.getNextSend();
        sendMsg_ = &(sendWindow_.sw_[send_index]);
        setSeq(sendWindow_.getSeqByIndex(send_index));
        sendMsg(sendMsg_->head.len);
        sendWindow_.updateNext(send_index);
        mutex_.unlock();
        while (sendWindow_.getNextSend() == sendWindow_.getDataEnd())
        {
            Sleep(WRITE_FILE_TIME);
            if(getSendOver())
                break;
        }
        // 如果没有空间，等待窗口大小更新
        if(sendWindow_.getWindow() == 0)
        {
            sendMsg_ = new fileMessage;
            sendMsg_->head.flag = PSH;
            while (sendWindow_.getWindow() == 0)
            {
                Sleep(KEEP_ALIVE_TIME);
                sendMsg(0, 0);
            }
            delete sendMsg_;
        }
    }
    wait_ACK.join();
    int try_disconnect = 0;
    while(disconnect() != RC::SUCCESS)
    {
        try_disconnect++;
        if(try_disconnect == MAX_RETRY_TIMES)
        {
            cout << "尝试断开连接失败" << endl;
            return RC::INTERNAL;
        }
        cout << "尝试重新断开连接" << endl;
    }
    gettimeofday(&end, NULL);
    double totalTime = end.tv_sec - start.tv_sec + (end.tv_usec - start.tv_usec) / 1000000.0;
    cout << "发送文件大小为 " << fileSize_ << " B" << endl;
    cout << "传输总用时为 " << totalTime << " s" << endl;
    cout << "平均吞吐率为 " << fileSize_ / totalTime * 8 / (1000 * 1000) << " Mbps" << endl;
    return RC::SUCCESS;
}