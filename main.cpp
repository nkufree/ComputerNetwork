#include <iostream>
#include "send_file.h"
#include "recv_file.h"
#include "defs.h"
using namespace std;

char sendIP[20] = {SEND_IP};
char routerIP[20] = {ROUTER_IP};
char recvIP[20] = {RECV_IP};

bool sendFile(char* fileName)
{
    SendFile* send = new SendFile(sendIP, routerIP, SEND_PORT, ROUTER_PORT);
    bool ret;
    ret = send->init();
    if(ret == false) goto send_falied;
    ret = send->send(fileName);
    if(ret == false) goto send_falied;
    delete send;
    return true;
send_falied:
    delete send;
    return false;
}

bool recvFile()
{
    RecvFile* recv = new RecvFile(routerIP, recvIP, ROUTER_PORT, RECV_PORT);
    bool ret;
    ret = recv->init();
    if(ret == false) goto recv_falied;
    ret = recv->recv();
    if(ret == false) goto recv_falied;
    delete recv;
    return true;
recv_falied:
    delete recv;
    return false;
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
        bool ret = sendFile(fileName);
        if(ret == false)
        {
            cout << "发送文件失败" << endl;
        }
        else
        {
            cout << "发送文件成功" << endl;
        }
    }
    else if(sel == 2)
    {
        bool ret = recvFile();
        if(ret == false)
        {
            cout << "接收文件失败" << endl;
        }
        else
        {
            cout << "接收文件成功" << endl;
        }
    }
    cout << endl << "按任意键退出……" << endl;
    cin.get();
    return 0;
}