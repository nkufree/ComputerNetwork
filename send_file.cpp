#include "send_file.h"
#include <iostream>
#include <time.h>
#include <sys/time.h>

using namespace std;

SendFile::SendFile(const char* sendAddr, const char* recvAddr, int sendPort, int recvPort)
{
    recvAddr_.sin_family = AF_INET;
    recvAddr_.sin_addr.S_un.S_addr = inet_addr(recvAddr);
    recvAddr_.sin_port = htons(recvPort);

    sendAddr_.sin_family = AF_INET;
    sendAddr_.sin_addr.s_addr = inet_addr(sendAddr);
    sendAddr_.sin_port = htons(sendPort);
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
    // rc = recv_message(len);
    // while(rc == RC::WAIT_TIME_ERROR)
    // {
    //     sendMsg(0, sendMsg_->head.seq);
    //     rc = recv_message(len);
    // }
    // LOG_MSG(rc, "第二次握手成功", "第二次握手失败");

    // rc = sendMsg();
    // LOG_MSG(rc, "第三次握手成功\n建立连接成功", "第一次握手失败");
    return RC::SUCCESS;
}

// bool SendFile::sendMsg(int len)
// {
//     if(len < 0) return 1;
//     cout << "[发送] [seq]=" << sendMsg_->head.seq << " [flag]=" << sendMsg_->head.flag << " [len]=" << len << endl;
//     sendMsg_->head.seq = getSeq();
//     sendMsg_->head.crc32 = crc32((unsigned char*)&(sendMsg_->head.flag),len + sizeof(info) - sizeof(info::crc32));
//     if(sendto(sock_, (char*)(sendMsg_), len + sizeof(info), 0, (sockaddr*)&recvAddr_, sizeof(recvAddr_)) == -1)
//         return 0;
//     return 1;
// }

// bool SendFile::resend_message(int len)
// {
//     if(len < 0) return 1;
//     cout << "[发送] [seq] = " << sendMsg_->head.seq << " [flag] = 0x" << hex << sendMsg_->head.flag << " [len] = " << dec << len << " " << stateName[state_] << endl;
//     if(sendto(sock_, (char*)(sendMsg_), len + sizeof(info), 0, (sockaddr*)&recvAddr_, sizeof(recvAddr_)) == -1)
//         return 0;
//     return 1;
// }

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
    if(recvMsg_->head.seq != sendMsg_->head.seq && recvMsg_->head.flag != (FIN | ACK))
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
    LOG_MSG(rc, "", "发送文件名错误")
    return rc;
}

int SendFile::getSeq()
{
    return seq_++ % 2;
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

    // rc = recv_message(len);
    // while (rc == RC::WAIT_TIME_ERROR)
    // {
    //     sendMsg(0, sendMsg_->head.seq);
    //     rc = recv_message(len);
    // }
    // LOG_MSG(rc, "第二次挥手成功", "第二次挥手失败");
    // if(state_ == CLOSED)
    // {
    //     cout << "关闭连接成功" << endl;
    //     return RC::SUCCESS;
    // }
    
    // rc = recv_message(len);
    // while (rc == RC::WAIT_TIME_ERROR)
    // {
    //     sendMsg(0, sendMsg_->head.seq);
    //     rc = recv_message(len);
    // }
    // LOG_MSG(rc, "第三次挥手成功", "第三次挥手失败");

    // rc = sendMsg();
    // LOG_MSG(rc, "第四次挥手成功\n关闭连接成功", "第四次挥手失败");
    return rc;
}

RC  SendFile::setFile(const char* fileName)
{
    sendFileStream_.open(fileName, ios::binary);
    sendFileStream_.seekg(0, ios::end);
    int wait_send_len = sendFileStream_.tellg();
    fileSize_ = wait_send_len;
    cout << "发送文件大小为 " << wait_send_len << " B" << endl;
    sendFileStream_.seekg(0, ios::beg);
    fileName_ = strdup(fileName);
    return RC::SUCCESS;
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
    int ret;
    int wait_send_len = fileSize_;
    while(wait_send_len > 0)
    {
        int send_len = wait_send_len <= MSS ? wait_send_len : MSS;
        sendMsg_->head.flag = PSH;
        sendFileStream_.read(sendMsg_->msg, send_len);
        rc = send_and_wait(send_len);
        if(recvMsg_->head.flag != ACK)
        {
            cout << "接收方响应连接错误" << endl;
            return RC::INTERNAL;
        }
        if(rc == RC::RESET)
        {
            return this->start();
        }
        else if(rc != RC::SUCCESS)
            return rc;
        wait_send_len -= send_len;
    }
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