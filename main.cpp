#include <iostream>
#include "send_file.h"
#include "recv_file.h"
#include "defs.h"
#include <memory>
#pragma comment(lib, "ws2_32.lib")
using namespace std;

char sendIP[20] = {SEND_IP};
char routerIP[20] = {ROUTER_IP};
char recvIP[20] = {RECV_IP};

RC sendFile(char* fileName)
{
    unique_ptr<SendFile> send(new SendFile(sendIP, routerIP, RECV_PORT, SEND_PORT));
    double loss;
    int delay;
    cout << "请输入丢包率(%):";
    cin >> loss;
    cout << "请输入延迟(ms):";
    cin >> delay;
    bool ret;
    RC rc;
    send->setLoss(loss);
    send->setDelay(delay);
    ret = send->init();
    if(ret == false)
        return RC::INTERNAL;
    send->setFile(fileName);
    rc = send->start();
    LOG_MSG(rc, "发送文件成功", "发送文件失败");
    return rc;
}

RC recvFile()
{
    unique_ptr<RecvFile> recv(new RecvFile(routerIP, recvIP, SEND_PORT, RECV_PORT));
    bool ret;
    RC rc = RC::SUCCESS;
    ret = recv->init();
    if(ret == false) 
        return RC::INTERNAL;
    rc = recv->start();
    LOG_MSG(rc, "接收文件成功", "接收文件失败");
    return rc;
}

int main(int argc, char* argv[])
{
    if(argc == 3)
    {
        strcpy(sendIP, argv[1]);
        strcpy(recvIP, argv[2]);
    }
    cout << "请选择发送或者接收，发送(1)，接收(2) ：";
    int sel;
    cin >> sel;
    cin.get();
    if(sel == 1)
    {
        char fileName[MSS] = {};
        cout << "请输入文件名：";
        cin.getline(fileName, MSS - 1);
        sendFile(fileName);
    }
    else if(sel == 2)
    {
        recvFile();
    }
    return 0;
}