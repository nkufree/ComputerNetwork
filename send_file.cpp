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
    if((sockSend_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
    {
        cout << "初始化套接字错误" << endl;
        return 0;
    }
    if(bind(sockSend_, (sockaddr*)&sendAddr_, sizeof(sendAddr_)) == SOCKET_ERROR)
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

bool SendFile::init_connect()
{
    sendMsg_->head.flag = SYN;
    if(!send_and_wait(0))
    {
        cout << "第一次握手失败" << endl;
        return 0;
    }
    if(recvMsg_->head.flag != (SYN | ACK))
    {
        cout << "第二次握手失败" << endl;
        return 0;
    }
    sendMsg_->head.flag = ACK;
    if(!send_message(0))
    {
        cout << "第三次握手失败" << endl;
        return 0;
    }
    cout << "建立连接成功" << endl;
    return 1;
}

bool SendFile::send_message(int len)
{
    if(len < 0) return 1;
    cout << "[发送] [seq]=" << sendMsg_->head.seq << " [flag]=" << sendMsg_->head.flag << " [len]=" << len << endl;
    sendMsg_->head.seq = get_seq();
    sendMsg_->head.crc32 = crc32((unsigned char*)&(sendMsg_->head.flag),len + sizeof(info) - sizeof(info::crc32));
    if(sendto(sockSend_, (char*)(sendMsg_), len + sizeof(info), 0, (sockaddr*)&recvAddr_, sizeof(recvAddr_)) == -1)
        return 0;
    return 1;
}

bool SendFile::resend_message(int len)
{
    if(len < 0) return 1;
    cout << "[重新发送] [seq]=" << sendMsg_->head.seq << " [flag]=" << sendMsg_->head.flag << " [len]=" << len << endl;
    if(sendto(sockSend_, (char*)(sendMsg_), len + sizeof(info), 0, (sockaddr*)&recvAddr_, sizeof(recvAddr_)) == -1)
        return 0;
    return 1;
}

int SendFile::recv_message()
{
    int len;
    fd_set rset;
    FD_ZERO(&rset);
    FD_SET(sockSend_, &rset);
    if(select(sockSend_ + 1, &rset, NULL, NULL, &tv_) > 0)
    {
        if((len = recvfrom(sockSend_, (char*)recvMsg_, sizeof(fileMessage), 0, (sockaddr*)&recvAddr_, &addrSize_)) == SOCKET_ERROR)
            return len;
        else 
            cout << "[接收] [seq]=" << recvMsg_->head.seq << " [flag]=" << recvMsg_->head.flag << " [len]=" << len - sizeof(info) << endl;
    }
    else
    {
        return WAIT_TIME_ERROR;
    }
    uint32_t check_crc32 = crc32((unsigned char*)&(recvMsg_->head.flag),len  - sizeof(info::crc32));
    if(check_crc32 != recvMsg_->head.crc32)
        return CHECK_ERROR;
    if(recvMsg_->head.seq != sendMsg_->head.seq && recvMsg_->head.flag != (FIN | ACK))
        return SEQ_ERROR;
    return len;
}

int SendFile::recv_overtime()
{
    int len;
    fd_set rset;
    FD_ZERO(&rset);
    FD_SET(sockSend_, &rset);
    if(select(sockSend_ + 1, &rset, NULL, NULL, &tv_) > 0)
    {
        len = recv_message();
        return len;
    }
    return WAIT_TIME_ERROR;
}

bool SendFile::send_file_name(const char* fileName)
{
    sendMsg_->head.flag = PSH;
    string path = fileName;
    size_t found = path.find_last_of("/\\");
    string file_name = path.substr(found + 1);
    strcpy(sendMsg_->msg, file_name.c_str());
    if(!send_and_wait(strlen(sendMsg_->msg)))
    {
        cout << "发送文件名错误" << endl;
        return 0;
    }
    return 1;
}

int SendFile::get_seq()
{
    return seq_++ % 2;
}

bool SendFile::send_and_wait(int len)
{
    timeval start, center, end;
    gettimeofday(&start, NULL);
    if(!send_message(len))
    {
        cout << "与接收端失去连接" << endl;
        return 0;
    }
    gettimeofday(&center, NULL);
    int recv_len, retry_times = 0;
    while (true)
    {
        if(retry_times >= MAX_RETRY_TIMES)
        {
            cout << "重传次数过多" << endl;
            return 0;
        }

        if((recv_len = recv_message()) == SOCKET_ERROR)
        {
            cout << "接受数据错误" << endl;
            return 0;
        }
        else if(recv_len == SEQ_ERROR)
        {
            // cout << "丢弃ACK" << endl;
            continue;
        }
        else if(recv_len == CHECK_ERROR)
        {
            // cout << "校验错误重传"  << endl;
            resend_message(len);
            continue;
        }
        else if(recv_len == WAIT_TIME_ERROR)
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
            resend_message(len);
            continue;
        }
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
    return 1;
}

bool SendFile::disconnect()
{
    sendMsg_->head.flag = FIN;
    if(!send_and_wait(0))
    {
        cout << "第一次挥手错误" << endl;
        return 0;
    }
    if(recvMsg_->head.flag != ACK)
    {
        cout << "接收方第二次挥手错误" << endl;
        return 0;
    }
    if(!send_and_wait(-1))
    {
        cout << "第三次挥手错误" << endl;
        return 0;
    }
    
    if(recvMsg_->head.flag != (FIN | ACK))
    {
        cout << "第三次挥手失败" << endl;
        return 0;
    }
    sendMsg_->head.flag = ACK;
    if(!send_message(0))
    {
        cout << "第四次挥手失败" << endl;
        return 0;
    }
    cout << "关闭连接成功" << endl;
    return 1;
}

bool SendFile::send(const char* fileName)
{
    timeval start, end;
    gettimeofday(&start, NULL);
    if(!init_connect())
        return 0;
    if(!send_file_name(fileName))
        return 0;
    sendFileStream_.open(fileName, ios::binary);
    sendFileStream_.seekg(0, ios::end);
    int wait_send_len = sendFileStream_.tellg();
    fileSize_ = wait_send_len;
    cout << "发送文件大小为 " << wait_send_len << " B" << endl;
    sendFileStream_.seekg(0, ios::beg);
    int ret;
    while(wait_send_len > 0)
    {
        int send_len = wait_send_len <= MSS ? wait_send_len : MSS;
        sendMsg_->head.flag = PSH;
        sendFileStream_.read(sendMsg_->msg, send_len);
        ret = send_and_wait(send_len);
        if(recvMsg_->head.flag != ACK)
        {
            cout << "接收方响应连接错误" << endl;
            return 0;
        }
        if(ret == false) return ret;
        wait_send_len -= send_len;
    }
    ret = disconnect();
    if(ret == false) return ret;
    gettimeofday(&end, NULL);
    double totalTime = end.tv_sec - start.tv_sec + (end.tv_usec - start.tv_usec) / 1000000.0;
    cout << "传输总用时为 " << totalTime << " s" << endl;
    cout << "平均吞吐率为 " << fileSize_ / totalTime * 8 / (1000 * 1000) << " Mbps" << endl;
    return 1;
}