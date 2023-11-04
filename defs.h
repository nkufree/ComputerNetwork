#ifndef __DEFS_H__
#define __DEFS_H__

#include <iostream>
#include <minwindef.h>
#include <cstdint>
#define RECV_PORT 9000
#define ROUTER_PORT 9250
#define SEND_PORT 9500
#define MSS 8192
#define MAX_RETRY_TIMES 10
#define WAIT_TIME 2000
#define SYN 0x2
#define ACK 0x10
#define PSH 0x8
#define FIN 0x1
#define RST 0x4
#define SEND_IP "127.0.0.1"
#define ROUTER_IP "127.0.0.1"
// #define RECV_IP "10.130.100.172"
#define RECV_IP "127.0.0.1"
#define BUFF_SIZE 65535
#define WINDOW_SIZE 400

enum class RC   // 错误类型
{
    SOCK_ERROR,
    CHECK_ERROR,
    WAIT_TIME_ERROR,
    SEQ_ERROR,
    STATE_ERROR,
    UNIMPLENTED,
    INTERNAL,
    RESET,
    SUCCESS,
};

enum slidingPos // 需要移动的滑动窗口指针
{
    S_START,
    S_NEXT,
    S_END,
};

enum states     // 状态
{
    CLOSED,         // 发送端和接收端发送完成后的状态
    LISTEN,         // 发送端和接收端的初始状态
    SYN_SENT,       // 发送端发送第一次握手（SYN）后进入的状态
    SYN_RCVD,   // 接收端发送第二次握手（SYN+ACK）后进入的状态
    ESTABLISHED,    // 数据传输状态，发送或接收到第三次挥手后进入的状态
    CLOSE_WAIT,     // 接收端发送第二次挥手（ACK）后进入的状态
    FIN_WAIT_1,     // 发送端发送第一次挥手（FIN+ACK）之后进入的状态
    LAST_ACK,       // 接收端发送（FIN+ACK）之后进入的状态
    FIN_WAIT_2,     // 发送端收到第二次挥手（ACK）后进入的状态
};

#pragma pack(1)
struct info
{
    uint32_t crc32;
    uint32_t seq;
    uint32_t ack;
    WORD flag;
    WORD win;
};
struct fileMessage
{
    struct info head;
    char msg[MSS];
};
#pragma pack()

#define LOG_MSG(rc, success, falied) \
    if(rc != RC::SUCCESS) \
    { \
        handleError(rc); \
        cout << falied << endl; \
        return rc; \
    } \
    else if(strcmp(success, "") != 0)\
        cout << success << endl; 

extern char stateName[][15];

void handleError(RC rc);

#endif