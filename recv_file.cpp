#include "recv_file.h"
#include <iostream>
#include <sys/time.h>
#include <io.h>
using namespace std;

RecvFile::RecvFile(const char* sendAddr, const char* recvAddr, int sendPort, int recvPort)
{
    recvAddr_.sin_family = AF_INET;
    recvAddr_.sin_addr.S_un.S_addr = inet_addr(recvAddr);
    recvAddr_.sin_port = htons(recvPort);

    sendAddr_.sin_family = AF_INET;
    sendAddr_.sin_addr.s_addr = inet_addr(sendAddr);
    sendAddr_.sin_port = htons(sendPort);
}

bool RecvFile::init()
{
    WSADATA wsa_data;
    if(WSAStartup(MAKEWORD(2,2), &wsa_data) != 0)
    {
        cout << "初始化网络错误" << endl;
        return 0;
    }
    if((sockRecv_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
    {
        cout << "初始化套接字错误" << endl;
        return 0;
    }
    if(bind(sockRecv_, (sockaddr*)&recvAddr_, sizeof(recvAddr_)) == SOCKET_ERROR)
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


bool RecvFile::init_connect()
{
    int len;
    if((len = recv_message()) == SOCKET_ERROR)
    {
        cout << "接受与发送方连接失败" << endl;
        return 0;
    }
    else
    {
        if(recvMsg_->head.flag != SYN)
        {
            cout << "接受与发送方连接失败" << endl;
            return 0;
        }
    }
    gettimeofday(&start_, NULL);
    sendMsg_->head.flag = SYN | ACK;
    sendMsg_->head.seq = seq_;
    if(!send_message(0))
    {
        cout << "发送连接建立请求失败" << endl;
        return 0;
    }
    if((len = recv_message()) == SOCKET_ERROR)
    {
        cout << "与发送方连接失败" << endl;
        return 0;
    }
    else
    {
        if(recvMsg_->head.flag != ACK)
        {
            cout << "与发送方连接失败" << endl;
            return 0;
        }
    }
    cout << "建立连接成功" << endl;
    return 1;
}

bool RecvFile::send_message(int len)
{
    sendMsg_->head.seq = recvMsg_->head.seq;
    sendMsg_->head.crc32 = crc32((unsigned char*)&(sendMsg_->head.flag),len + sizeof(info) - sizeof(info::crc32));
    if(sendto(sockRecv_, (char*)(sendMsg_), len + sizeof(info), 0, (sockaddr*)&sendAddr_, sizeof(sendAddr_)) == -1)
        return 0;
    return 1;
}

int RecvFile::recv_message()
{
    int len;
    while(true)
    {
        if((len = recvfrom(sockRecv_, (char*)recvMsg_, sizeof(fileMessage), 0, (sockaddr*)&recvAddr_, &addrSize_)) == SOCKET_ERROR)
            return len;
        uint32_t check_crc32 = crc32((unsigned char*)&(recvMsg_->head.flag),len  - sizeof(info::crc32));
        if(check_crc32 != recvMsg_->head.crc32)
            continue;
        if(recvMsg_->head.seq == seq_)
        {
            sendMsg_->head.flag = ACK;
            send_message(0);
            continue;
        }
        break;
    }
    seq_ = recvMsg_->head.seq;
    return len;
}

bool RecvFile::recv_file_name()
{
    int len;
    if((len = recv_message()) == SOCKET_ERROR)
    {
        cout << "与发送方断开连接" << endl;
        return 0;
    }
    else
    {
        if(recvMsg_->head.flag != PSH)
        {
            cout << "发送方发送数据错误" << endl;
            return 0;
        }
        ((char*)recvMsg_)[len] = '\0';
        seq_ = recvMsg_->head.seq;
        cout << "收到文件名：" << recvMsg_->msg << endl;
    }
    sendMsg_->head.flag = ACK;
    send_message(0);
    if(access("recv", 0) == -1)
    {
        system("md recv");
    }
    char recvDir[MAX_SEND_SIZE + 8] = {"recv\\"};
    strcat(recvDir, recvMsg_->msg);
    recvFileStream_.open(recvDir, ios::binary);
    return 1;
}

bool RecvFile::wait_and_send()
{
    int len;
    while(true)
    {
        len = recv_message();
        if(len == SOCKET_ERROR)
        {
            cout << "与发送方断开连接" << endl;
            return 0;
        }
        else
        {
            if(recvMsg_->head.flag == PSH)
            {
                seq_ = recvMsg_->head.seq;
                recvFileStream_.write(recvMsg_->msg, len - sizeof(info));
                recvFileStream_.flush();
                sendMsg_->head.flag = ACK;
                if(!send_message(0))
                {
                    cout << "发送ACK错误" << endl;
                    return 0;
                }
            }
            else if(recvMsg_->head.flag == FIN)
            {
                return disconnect();
            }
        }
    }
    return 0;
}

bool RecvFile::disconnect()
{
    int len;
    sendMsg_->head.flag = ACK;
    if(!send_message(0))
    {
        cout << "第二次挥手失败" << endl;
        return 0;
    }
    sendMsg_->head.flag = FIN | ACK;
    if(!send_message(0))
    {
        cout << "第三次挥手失败" << endl;
        return 0;
    }
    if((len = recv_message()) == SOCKET_ERROR)
    {
        cout << "第四次挥手失败" << endl;
        return 0;
    }
    else
    {
        if(recvMsg_->head.flag != ACK)
        {
            cout << "第四次挥手失败" << endl;
            return 0;
        }
    }
    cout << "关闭连接成功" << endl;
    return 1;
}

bool RecvFile::recv()
{
    if(!init_connect())
        return 0;
    if(!recv_file_name())
        return 0;
    if(!wait_and_send())
        return 0;
    gettimeofday(&end_, NULL);
    cout << "接收等待时长为 " << end_.tv_sec - start_.tv_sec + (end_.tv_usec - start_.tv_usec) / 1000000.0 << " s" << endl;
    return 1;
}