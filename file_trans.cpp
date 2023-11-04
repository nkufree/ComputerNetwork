#include "file_trans.h"
#include <iostream>
#include "crc32.h"

using namespace std;
FileTrans::FileTrans(/* args */)
{
}

FileTrans::~FileTrans()
{
    if(sendMsg_ != nullptr)
        delete sendMsg_;
    if(recvMsg_ != nullptr)
        delete recvMsg_;
}

RC FileTrans::open()
{
    state_ = LISTEN;
    return RC::SUCCESS;
}

RC FileTrans::recvMsg(int &len)
{
    RC rc = RC::SUCCESS;
    len = recvfrom(sock_, (char*)recvMsg_, sizeof(fileMessage), 0, (sockaddr*)&recvAddr_, &addrSize_);
    cout << "[ recv ] [ seq ] = " << recvMsg_->head.seq << " [ flag ] = 0x" << hex << recvMsg_->head.flag << " [ len ] = " << dec << len - sizeof(info)  << " [ state ] = " << stateName[state_] << endl;
    if(len == SOCKET_ERROR)
        return RC::SOCK_ERROR;
    uint32_t check_crc32 = crc32((unsigned char*)&(recvMsg_->head.flag),len  - sizeof(info::crc32));
    if(check_crc32 != recvMsg_->head.crc32)
        return RC::CHECK_ERROR;
    WORD& flag = recvMsg_->head.flag;
    if(flag == RST)
    {
        state_ = LISTEN;
        return RC::SUCCESS;
    }
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
            break;
        case SYN_RCVD:      // 接收端发送第二次握手
            if(flag == ACK)
                state_ = ESTABLISHED;
            break;
        case ESTABLISHED:   // 数据传输状态
            if(flag == FIN)
                sendMsg_->head.flag = ACK;
            else if(flag == PSH)
                sendMsg_->head.flag = ACK;
            break;
        case CLOSE_WAIT:    // 接收端发送第二次挥手后，应该发送FIN+ACK而不是接收
            return RC::STATE_ERROR;
        case FIN_WAIT_1:    // 第一次挥手后
            if(flag == ACK)
                state_ = FIN_WAIT_2;
            else if(flag == (FIN | ACK))
                sendMsg_->head.flag = ACK;
            break;
        case LAST_ACK:      // 第三次挥手后
            if(flag == ACK)
                state_ = CLOSED;
            break;
        case FIN_WAIT_2:    // 收到第二次挥手后，等待第三次挥手
            if(flag == (FIN | ACK))
                sendMsg_->head.flag = ACK;
            break;
        default:
            return RC::INTERNAL;
    }
    
    return rc;
}

RC FileTrans::sendMsg(int len, int seq)
{
    if(len < 0) return RC::INTERNAL;
    if(seq == -1)
        sendMsg_->head.seq = getSeq();
    sendMsg_->head.crc32 = crc32((unsigned char*)&(sendMsg_->head.flag),len + sizeof(info) - sizeof(info::crc32));
    if(sendto(sock_, (char*)(sendMsg_), len + sizeof(info), 0, (sockaddr*)&sendAddr_, sizeof(sendAddr_)) == -1)
        return RC::SOCK_ERROR;
    cout << "[ send ] [ seq ] = " << sendMsg_->head.seq << " [ flag ] = 0x" << hex << sendMsg_->head.flag << " [ len ] = " << dec << len << " [ state ] = " << stateName[state_] << endl;
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