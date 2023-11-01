#ifndef __DEFS_H__
#define __DEFS_H__

#include <iostream>
#include <minwindef.h>
#define RECV_PORT 9000
#define SEND_PORT 9500
#define MAX_SEND_SIZE 1024
#define MAX_RETRY_TIMES 10
#define WAIT_TIME 100
#define CHECK_ERROR (-10)
#define WAIT_TIME_ERROR (-11)
#define SEQ_ERROR (-12)
#define SYN 0x2
#define ACK 0x10
#define PSH 0x8
#define FIN 0x1
#define SEND_IP "127.0.0.1"
// #define RECV_IP "10.130.100.172"
#define RECV_IP "127.0.0.1"
#pragma pack(1)
struct info
{
    uint32_t crc32;
    unsigned char flag;
    unsigned char seq;
};
struct fileMessage
{
    struct info head;
    char msg[MAX_SEND_SIZE];
};
#pragma pack()

#endif