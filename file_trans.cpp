#include "file_trans.h"
#include <iostream>
#include "crc32.h"
#include <windows.h>

using namespace std;
FileTrans::FileTrans(const char* sendAddr, const char* recvAddr, int sendPort, int recvPort)
{
    recvAddr_.sin_family = AF_INET;
    recvAddr_.sin_addr.S_un.S_addr = inet_addr(recvAddr);
    recvAddr_.sin_port = htons(recvPort);

    sendAddr_.sin_family = AF_INET;
    sendAddr_.sin_addr.s_addr = inet_addr(sendAddr);
    sendAddr_.sin_port = htons(sendPort);
    seq_ = 0;
    ack_ = 0;
    loss_count_ = 0;
    delay_count_ = 0;
    loss_num_ = 0xFFFFFFF;
    delay_ = 0;
}

FileTrans::~FileTrans()
{
}

RC FileTrans::open()
{
    state_ = LISTEN;
    return RC::SUCCESS;
}

int FileTrans::getSeq(bool inc)
{
    // lock_guard<mutex> lock(seq_mutex_);
    if(inc)
        return seq_++;
    else
        return seq_;
}

void FileTrans::setSeq(uint32_t seq)
{
    // lock_guard<mutex> lock(seq_mutex_);
    seq_ = seq;
}

int FileTrans::getAck()
{
    // lock_guard<mutex> lock(ack_mutex_);
    return ack_;
}

void FileTrans::setAck(uint32_t ack)
{
    // lock_guard<mutex> lock(ack_mutex_);
    ack_ = ack;
}

RC FileTrans::recvMsg(int &len)
{
    RC rc = RC::SUCCESS;
    len = recvfrom(sock_, (char*)recvMsg_, sizeof(fileMessage), 0, (sockaddr*)&recvAddr_, &addrSize_);
    print_mutex_.lock();
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_INTENSITY | FOREGROUND_RED |
		FOREGROUND_GREEN | FOREGROUND_BLUE);
    cout << dec << "[ recv ] [ seq ] = " << (int)recvMsg_->head.seq 
    << " [ ack ] = " << (int)recvMsg_->head.ack
    << " [ flag ] = 0x" << hex << (int)recvMsg_->head.flag 
    << " [ len ] = " << dec << len - sizeof(info)  
    << " [ win ] = " << (int)recvMsg_->head.win 
    << " [ state ] = " << stateName[state_] << endl;
    //<< " [ crc32 ] = " << hex << recvMsg_->head.crc32 << endl;
    print_mutex_.unlock();
    if(len == SOCKET_ERROR)
        return RC::SOCK_ERROR;
    uint32_t check_crc32 = crc32((unsigned char*)&(recvMsg_->head.flag),len  - sizeof(info::crc32));
    // if(check_crc32 != recvMsg_->head.crc32)
    //     return RC::CHECK_ERROR;
    auto& flag = recvMsg_->head.flag;
    switch(state_)
    {
        case CLOSED:        // 传输已完成，需要重新建立连接
            return RC::STATE_ERROR;
        case LISTEN:        // 接收端正在等待第一次握手
            if(flag == SYN)
                sendMsg_->head.flag = SYN | ACK;
            break;
        case SYN_SENT:      // 发送端发送完第一次握手
            if(flag == SYN)
                sendMsg_->head.flag = SYN | ACK;
            else if(flag == (SYN | ACK))
                sendMsg_->head.flag = ACK;
            else state_ = LISTEN;
            break;
        case SYN_RCVD:      // 接收端发送第二次握手
            if(flag == ACK)
                state_ = ESTABLISHED;
            else state_ = LISTEN;
            break;
        case ESTABLISHED:   // 数据传输状态
            if(flag == FIN)
                sendMsg_->head.flag = ACK;
            else if(flag == PSH && getType() == F_RECV)
                sendMsg_->head.flag = ACK;
            break;
        case CLOSE_WAIT:    // 接收端发送第二次挥手后，应该发送FIN+ACK而不是接收
            return RC::STATE_ERROR;
        case FIN_WAIT_1:    // 第一次挥手后
            if(flag == ACK)
                state_ = FIN_WAIT_2;
            else if(flag == (FIN | ACK))
                sendMsg_->head.flag = ACK;
            else
                state_ = ESTABLISHED;
            break;
        case LAST_ACK:      // 第三次挥手后
            if(flag == ACK)
                state_ = CLOSED;
            else state_ = ESTABLISHED;
            break;
        case FIN_WAIT_2:    // 收到第二次挥手后，等待第三次挥手
            if(flag == (FIN | ACK))
                sendMsg_->head.flag = ACK;
            else state_ = ESTABLISHED;
            break;
        default:
            return RC::INTERNAL;
    }
    setAck(recvMsg_->head.seq + 1);
    return rc;
}

RC FileTrans::sendMsg(int len, int seq)
{
    loss_count_++;
    delay_count_++;
    if(len < 0) return RC::INTERNAL;
    if(seq == -1)
        sendMsg_->head.seq = getSeq();
    else 
        sendMsg_->head.seq = seq;
    sendMsg_->head.ack = getAck();
    sendMsg_->head.win = getWin();
    sendMsg_->head.crc32 = crc32((unsigned char*)&(sendMsg_->head.flag),len + sizeof(info) - sizeof(info::crc32));
    if(delay_count_ % 100 == 0)
        Sleep(delay_);
    if(loss_count_ % loss_num_ != 0)
    {
        if(sendto(sock_, (char*)(sendMsg_), len + sizeof(info), 0, (sockaddr*)&sendAddr_, sizeof(sendAddr_)) == -1)
            return RC::SOCK_ERROR;
    }
    
    print_mutex_.lock();
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_INTENSITY | FOREGROUND_GREEN);
    cout << dec << "[ send ] [ seq ] = " << (int)sendMsg_->head.seq 
    << " [ ack ] = " << (int)sendMsg_->head.ack
    << " [ flag ] = 0x" << hex << (int)sendMsg_->head.flag 
    << " [ len ] = " << dec << len 
    << " [ win ] = " << (int)sendMsg_->head.win 
    << " [ state ] = " << stateName[state_] << endl;
    //<< " [ crc32 ] = 0x" << hex << sendMsg_->head.crc32 << endl;
    print_mutex_.unlock();
    switch(state_)
    {
        case LISTEN:
            if(getType() == Type::F_SEND)
                state_ = SYN_SENT;
            else if(getType() == Type::F_RECV)
                state_ = SYN_RCVD;
            break;
        case SYN_SENT:
            if(sendMsg_->head.flag == (SYN | ACK))
                state_ = SYN_RCVD;
            else if(sendMsg_->head.flag = ACK)
                state_ = ESTABLISHED;
            break;
        case ESTABLISHED:
            if(recvMsg_->head.flag == FIN)
                state_ = CLOSE_WAIT;
            else if(sendMsg_->head.flag == FIN)
                state_ = FIN_WAIT_1;
            break;
        case CLOSE_WAIT:
            state_ = LAST_ACK;
            break;
        case FIN_WAIT_1:
        case FIN_WAIT_2:
            state_ = CLOSED;
            break;
        default:
            break;
    }
    
    return RC::SUCCESS;
}